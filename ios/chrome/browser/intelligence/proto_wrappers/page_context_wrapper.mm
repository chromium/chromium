// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/barrier_closure.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/memory/weak_ptr.h"
#import "base/not_fatal_until.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/token.h"
#import "base/values.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/core/page_content_proto_serializer.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/annotated_page_content_extraction_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/frame_grafter.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_metrics.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"
#import "url/origin.h"
#import "url/url_constants.h"

// TODO(crbug.com/458081684): Move away from all autofill dependencies once
// the migration in ios/web is done for frame registration.
namespace {

// The default Page Context execution timeout.
base::TimeDelta kDefaultPageContextTimeout = base::Seconds(1);

// Url used for for data urls.
constexpr const char kDataUrl[] = "data:";

// The timeout for waiting for remote frame registration.
base::TimeDelta kRegistrationTimeout = base::Milliseconds(200);

// The key for whether the PageContext should be detached. The value is a
// bool.
constexpr const char kShouldDetachPageContext[] = "shouldDetachPageContext";

// The key for the current node's innerText in the JavaScript object. The
// value is a string.
constexpr const char kCurrentNodeInnerTextDictKey[] = "currentNodeInnerText";

// The key for the children frames in the JavaScript object. The value is an
// array of objects.
constexpr const char kChildrenFramesDictKey[] = "children";

// The key for the PageInteractionInfo of the main frame.
constexpr const char kPageInteractionInfoDictKey[] = "pageInteractionInfo";

// The key for the ViewportGeometry of the main frame.
constexpr const char kViewportGeometryDictKey[] = "viewportGeometry";

// The key for the links of the frame in the JavaScript object. The value is
// an array of objects.
constexpr const char kFrameLinksDictKey[] = "links";

// The key for a link's HREF/URL field in the JavaScript object. The value is
// a string.
constexpr const char kLinkHREFDictKey[] = "href";

// The key for a link's innerText in the JavaScript object. The value is a
// string.
constexpr const char kLinkTextDictKey[] = "linkText";

// The key for the remote frame token.
constexpr const char kRemoteFrameTokenKey[] = "remoteToken";

// The JavaScript to be executed on each WebState's WebFrames, which retrieves
// the innerText of the document body, and recursively traverses through
// same-origin nested iframes to retrieve their innerTexts as well,
// constructing a tree structure. iframes are marked as processed with a nonce
// to avoid duplicate text from frames, but only for the current run. Early
// returns if the PageContext should be detached, or the frame is not the
// top-most same-origin frame.
constexpr const char16_t* kInnerTextTreeJavaScript = uR"DELIM(
(() => {
    // Checks whether the PageContext should be detached.
    const shouldDetachPageContext = () => {
      // PageContext detachment logic injected below.
      $1
    };

    // If the PageContext should be detached, early return.
    if (shouldDetachPageContext()) {
        return { shouldDetachPageContext: true };
    }

    // The script should only run if it has no same-origin parent. (The script
    // should only start execution on top-most nodes of a given origin).
    if (window.self !== window.top &&
        location.ancestorOrigins?.[0] === location.origin) {
        // Not the top-most same-origin frame, early exit.
        return null;
    }

    // Recursively constructs the innerText tree for the passed node and its
    // children same-origin iframes.
    const constructSameOriginInnerTextTree = (node, frameURL, frameTitle, nonceAttributeValue) => {
        // Early return if the node is null or already processed.
        if (!node || node.getAttribute('data-__gCrWeb-innerText-processed') === nonceAttributeValue) {
            return null;
        }

        // Mark node as processed.
        node.setAttribute('data-__gCrWeb-innerText-processed', nonceAttributeValue);

        // Get all nested iframes within the current node.
        const nestedIframes = node.getElementsByTagName('iframe');
        const childNodeInnerTexts = [...nestedIframes].map(iframe => {
            if (!iframe) {
                return null;
            }

            // Try to access the iframe's body, failure is possible (cross-origin iframes).
            let iframeBody;
            let iframeTitle;
            try {
                const contentDoc = iframe.contentDocument;
                iframeBody = contentDoc ? contentDoc.body : null;
                iframeTitle = contentDoc ? contentDoc.title : '';
            } catch (error) {
                return null;
            }

            // Recursively construct the innerText tree for the iframe's body.
            return iframeBody ? constructSameOriginInnerTextTree(iframeBody,
                iframe.src, iframeTitle, nonceAttributeValue) : null;
        });

        const result = {
            currentNodeInnerText: node.innerText,
            children: childNodeInnerTexts.filter(item => item !== null),
            sourceUrl: frameURL,
            title: frameTitle,
        };

        // Anchor tag retrieval logic injected below.
        $2

        return result;
    };

    return constructSameOriginInnerTextTree(document.body, window.location.href, document.title, "$3");
})();
  )DELIM";

// The JavaScript to be executed in each WebFrame which gets all of a frame's
// anchor tags and adds them to an array with their corresponding URL and
// textContent (which includes all text, including text that is not visually
// rendered). Injected into the main script.
constexpr const char16_t* kAnchorTagsJavaScript = uR"DELIM(
// Add all the frame's anchor tags to a links array with their HREF/URL and
// textContent.
const linksArray = [];
const anchorElements = node.querySelectorAll('a[href]');
anchorElements.forEach((anchor) => {
    linksArray.push({
        href: (anchor instanceof SVGAElement) ? anchor.href.baseVal : anchor.href,
        linkText: anchor.textContent
    });
});

result.links = linksArray;
  )DELIM";

}  // namespace

@implementation PageContextWrapper {
  base::WeakPtr<web::WebState> _webState;

  // The amount of async tasks this specific instance of the PageContext wrapper
  // needs to complete before executing the `completionCallback`.
  NSInteger _asyncTasksToComplete;

  // The timer which keeps track of the overall execution timeout.
  base::OneShotTimer _timeoutTimer;

  // The timer which keeps track of the registration timeout.
  base::OneShotTimer _registrationTimeoutTimer;

  // The root node of the PageContext's AnnotatedPageContent (APC) tree. This
  // tree is constructed on the fly as values are returned from JavaScript.
  std::unique_ptr<optimization_guide::proto::AnnotatedPageContent> _rootAPCNode;

  // The string which aggregates all iframes' innerTexts.
  std::unique_ptr<std::string> _innerText;

  // Whether the PageContext should be detached. Likely a protected page.
  BOOL _forceDetachPageContext;

  // Whether the PageContext is not extractable.
  BOOL _notExtractable;

  // The callback to execute once all async work is complete, whichs
  // relinquishes ownership of the PageContext proto to the callback's handler.
  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      _completionCallback;

  // Unique pointer to the PageContext proto.
  std::unique_ptr<optimization_guide::proto::PageContext> _pageContext;

  // The current PageContext instance's metrics logger. Only created when async
  // tasks execution is started.
  PageContextWrapperMetrics* _pageContextMetrics;

  // Configuration for page context extraction. Using optional avoids using
  // the constructor.
  std::optional<PageContextWrapperConfig> _config;

  // Graft frames across origins.
  FrameGrafter _grafter;

  // TODO(crbug.com/507879464): Add a metric to record the number of focused
  // frames per request to detect anomalies.
  // The identifiers of the traversed frames that reported being focused.
  std::vector<FrameFocusInfo> _focusedFrameInfos;

  // Caches string representations of autofill sections to integers during
  // extraction to ensure consistency across the APC proto.
  base::flat_map<std::string, uint32_t> _autofillSectionNumbers;

  // Whether the registration wait has completed or timed out.
  BOOL _registrationCompletedOrTimedOut;

  // Enforces that execution only happens once.
  BOOL _executionStarted;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                          config:(PageContextWrapperConfig)config
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  CHECK(webState);

  self = [super init];
  if (self) {
    _asyncTasksToComplete = 0;
    _webState = webState->GetWeakPtr();
    _config = config;
    _completionCallback = std::move(completionCallback);

    // Create the PageContext proto/object.
    _pageContext = std::make_unique<optimization_guide::proto::PageContext>();
    GURL url = _webState->GetVisibleURL();
    if (url.SchemeIs(url::kDataScheme)) {
      _pageContext->set_url(kDataUrl);
    } else {
      _pageContext->set_url(url.spec());
    }
    _pageContext->set_title(base::UTF16ToUTF8(_webState->GetTitle()));
  }
  return self;
}

- (instancetype)initWithWebState:(web::WebState*)webState
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  return [self initWithWebState:webState
                         config:PageContextWrapperConfigBuilder().Build()
             completionCallback:std::move(completionCallback)];
}

- (void)dealloc {
  _timeoutTimer.Stop();
  _registrationTimeoutTimer.Stop();
  [self stopTextHighlighting];
}

- (void)populatePageContextFieldsAsync {
  [self populatePageContextFieldsAsyncWithTimeout:kDefaultPageContextTimeout];
}

- (void)populatePageContextFieldsAsyncWithTimeout:(base::TimeDelta)timeout {
  CHECK(!_executionStarted) << "A PageContextWrapper should only be used once.";
  _executionStarted = YES;

  if (_isLowPriorityExtraction) {
    __weak PageContextWrapper* weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf populateAsyncFields:timeout];
        }));
    return;
  }

  [self populateAsyncFields:timeout];
}

#pragma mark - Setters

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetSnapshot:(BOOL)shouldGetSnapshot {
  if (_shouldGetSnapshot == shouldGetSnapshot) {
    return;
  }

  _shouldGetSnapshot = shouldGetSnapshot;
  _asyncTasksToComplete += shouldGetSnapshot ? 1 : -1;
}

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetFullPagePDF:(BOOL)shouldGetFullPagePDF {
  if (_shouldGetFullPagePDF == shouldGetFullPagePDF) {
    return;
  }

  _shouldGetFullPagePDF = shouldGetFullPagePDF;
  _asyncTasksToComplete += shouldGetFullPagePDF ? 1 : -1;
}

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetAnnotatedPageContent:(BOOL)shouldGetAnnotatedPageContent {
  if (_shouldGetAnnotatedPageContent == shouldGetAnnotatedPageContent) {
    return;
  }

  _shouldGetAnnotatedPageContent = shouldGetAnnotatedPageContent;

  // Only update `_asyncTasksToComplete` if `_shouldGetInnerText` is false,
  // since they both affect the same async task.
  if (!_shouldGetInnerText) {
    _asyncTasksToComplete += shouldGetAnnotatedPageContent ? 1 : -1;
  }
}

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetInnerText:(BOOL)shouldGetInnerText {
  if (shouldGetInnerText == _shouldGetInnerText) {
    return;
  }

  _shouldGetInnerText = shouldGetInnerText;

  // Only update `_asyncTasksToComplete` if `_shouldGetAnnotatedPageContent` is
  // false, since they both affect the same async task.
  if (!_shouldGetAnnotatedPageContent) {
    _asyncTasksToComplete += shouldGetInnerText ? 1 : -1;
  }
}

#pragma mark - Private

// Cancels the registration timeout.
- (void)cancelRegistrationTimeout {
  _registrationTimeoutTimer.Stop();
}

// Completes the registration wait then calls `barrier`.
- (void)completeRegistrationWaitWithBarrier:(base::RepeatingClosure)barrier {
  if (_registrationCompletedOrTimedOut) {
    // Avoid double completion.
    return;
  }
  _registrationCompletedOrTimedOut = YES;
  [self cancelRegistrationTimeout];
  barrier.Run();
}

// Returns the registrar.
- (autofill::ChildFrameRegistrar*)frameRegistrar {
  // Returns nullptr if the WebState has been destroyed during async operations.
  if (!_webState) {
    return nullptr;
  }
  return autofill::ChildFrameRegistrar::GetOrCreateForWebState(_webState.get());
}

// Returns the WebFramesManager to use for executing Page Context script on
// frames.
- (web::WebFramesManager*)webFramesManager {
  web::ContentWorld world =
      base::FeatureList::IsEnabled(kPageContextExtractorRefactored)
          ? PageContextExtractorJavaScriptFeature::GetInstance()
                ->GetSupportedContentWorld()
          : web::ContentWorld::kPageContentWorld;
  return _webState->GetWebFramesManager(world);
}

// Populates the fields of the PageContext proto which necessitate async
// calls.
- (void)populateAsyncFields:(base::TimeDelta)timeout {
  CHECK_GE(_asyncTasksToComplete, 0);
  _pageContextMetrics = [[PageContextWrapperMetrics alloc]
      initWithAPCConfigVariant:_config->GetApcConfigVariant()];

  if (!_webState || _asyncTasksToComplete == 0) {
    [self asyncWorkCompletedForPageContext];
    return;
  }

  if (!CanExtractPageContextForWebState(_webState.get())) {
    _notExtractable = YES;
    [self asyncWorkCompletedForPageContext];
    return;
  }

  __weak PageContextWrapper* weakSelf = self;

  // Start the timer.
  _timeoutTimer.Start(FROM_HERE, timeout, base::BindOnce(^{
                        [weakSelf onTimeout];
                      }));

  // Use a `BarrierClosure` to ensure all async tasks are completed before
  // executing the overall completion callback. The BarrierClosure will wait
  // until the `pageContextBarrier` callback is itself run
  // `_asyncTasksToComplete` times, then post the completion handler to
  // execute on the next loop of the current sequence.
  base::RepeatingClosure pageContextBarrier = base::BarrierClosure(
      _asyncTasksToComplete,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(^{
                           [weakSelf asyncWorkCompletedForPageContext];
                         })));

  // Asynchronous work. *IMPORTANT NOTES*:
  // When adding async tasks below, an accompanying setter should also be
  // created to follow the disabled-by-default pattern (which
  // increments/decrements `_asyncTasksToComplete` accordingly). Also, if a
  // given task is enabled, every code path for that task should eventually
  // execute the `pageContextBarrier` callback, otherwise the `BarrierClosure`
  // will never execute its completion block.

  if (_shouldGetSnapshot) {
    [self processSnapshotWithBarrier:pageContextBarrier];
  }

  if (_shouldGetAnnotatedPageContent || _shouldGetInnerText) {
    [self processAnnotatedPageContentWithBarrier:pageContextBarrier];
  }

  // Create full page PDF representation of the WebState, if enabled.
  if (_shouldGetFullPagePDF) {
    [_pageContextMetrics executionStartedForTask:PageContextTask::kPDF];

    _webState->CreateFullPagePdf(base::BindOnce(^(NSData* PDFData) {
      [weakSelf encodeAndSetFullPagePDF:PDFData];
      pageContextBarrier.Run();
    }));
  }
}

// Retrieve WebState snapshot. The barrier's callback will be executed for all
// codepaths in this method.
- (void)processSnapshotWithBarrier:(base::RepeatingClosure)barrier {
  [_pageContextMetrics executionStartedForTask:PageContextTask::kScreenshot];

  __weak PageContextWrapper* weakSelf = self;
  auto callback = ^(UIImage* image) {
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    if ([strongSelf shouldUpdateSnapshotWithImage:image]) {
      [strongSelf updateSnapshotWithBarrier:barrier];
      return;
    }

    [strongSelf encodeImageAndSetTabScreenshot:image];
    barrier.Run();
  };

  // If the WebState is currently visible, update the snapshot in case the
  // user was scrolling, otherwise retrieve the latest version in cache or on
  // disk.
  if (_webState->IsVisible()) {
    auto updateSnapshotCallback =
        base::BindOnce(^(std::optional<int> result_matches) {
          // TODO(crbug.com/401282824): Log the matches count to measure text
          // highlighting precision.
          [weakSelf updateSnapshotWithCallback:callback];
        });

    // If there is text to highlight, do it before capturing the screenshot.
    if (_textToHighlight != nil) {
      web::FindInPageJavaScriptFeature* findInPageFeature =
          web::FindInPageJavaScriptFeature::GetInstance();
      web::WebFrame* mainFrame =
          _webState
              ->GetWebFramesManager(
                  findInPageFeature->GetSupportedContentWorld())
              ->GetMainWebFrame();

      findInPageFeature->Search(mainFrame,
                                base::SysNSStringToUTF8(_textToHighlight),
                                std::move(updateSnapshotCallback));
    } else {
      std::move(updateSnapshotCallback).Run(std::nullopt);
    }
  } else {
    SnapshotTabHelper::FromWebState(_webState.get())
        ->RetrieveColorSnapshot(callback);
  }
}

// Helper to extract page context for a given frame.
// TODO(crbug.com/495446456): Clean up once the JSON experiment is done.
- (void)extractPageContextForFrame:(web::WebFrame*)frame
                       isMainFrame:(BOOL)isMainFrame
                             nonce:(const std::string&)nonce
       annotatedPageContentBarrier:
           (base::RepeatingClosure)annotatedPageContentBarrier {
  PageContextExtractorJavaScriptFeature* extractorFeature =
      PageContextExtractorJavaScriptFeature::GetInstance();

  __weak PageContextWrapper* weakSelf = self;

  // Use a timeout for the JS call larger than the wrapper's timer timeout
  // since this is the preferred way of timing out the dispatched jobs
  // (which will return a PageContextWrapperError::kTimeout error instead of
  // empty results).
  base::TimeDelta jsTimeout = _timeoutTimer.GetCurrentDelay() * 2;

  if (IsPageContextIPCOptimizationEnabled()) {
    // Callback to aggregate values from the JS execution via JSON string
    // parsing.
    auto callbackJson = [](PageContextWrapper* weakWrapper,
                           base::RepeatingClosure barrier, BOOL isMainFrame,
                           const url::Origin& securityOrigin,
                           std::optional<autofill::LocalFrameToken> frameId,
                           std::optional<base::Value> value) {
      // TODO(crbug.com/454261374): Remove `withError` from args once we
      // cleanup the old code. Can't provide an error object since the
      // javascript feature doesn't support that.
      [weakWrapper aggregateJavaScriptValue:value ? &value.value() : nullptr
                                  withError:nil
                                isMainFrame:isMainFrame
                             securityOrigin:securityOrigin
                            localFrameToken:frameId];
      barrier.Run();

      // Defer destruction of the base::DictValue to a background thread to
      // prevent blocking the main thread. The object is self-contained and
      // thread-safe.
      if (value) {
        base::ThreadPool::PostTask(
            FROM_HERE, {base::TaskPriority::BEST_EFFORT},
            base::BindOnce([](base::Value v) {}, std::move(*value)));
      }
    };

    extractorFeature->ExtractPageContextJSON(
        frame, _config->graft_cross_origin_frame_content(),
        _config->use_rich_extraction(),
        _config->use_rich_extraction_with_actionable(),
        _config->extract_paid_content(),
        _config->attempt_paid_content_json_fixing(), nonce, jsTimeout,
        base::BindOnce(
            callbackJson, weakSelf, annotatedPageContentBarrier, isMainFrame,
            frame->GetSecurityOrigin(),
            DeserializeFrameIdAsLocalFrameToken(frame->GetFrameId())));
  } else {
    // Callback to aggregate values from the JS execution via the base value
    // received directly from WebKit.
    auto callback = [](PageContextWrapper* weakWrapper,
                       base::RepeatingClosure barrier, BOOL isMainFrame,
                       const url::Origin& securityOrigin,
                       std::optional<autofill::LocalFrameToken> frameId,
                       const base::Value* value) {
      // TODO(crbug.com/454261374): Remove `withError` from args once we
      // cleanup the old code. Can't provide an error object since the
      // javascript feature doesn't support that.
      [weakWrapper aggregateJavaScriptValue:value
                                  withError:nil
                                isMainFrame:isMainFrame
                             securityOrigin:securityOrigin
                            localFrameToken:frameId];
      barrier.Run();
    };

    extractorFeature->ExtractPageContext(
        frame, _config->graft_cross_origin_frame_content(),
        _config->use_rich_extraction(),
        _config->use_rich_extraction_with_actionable(),
        _config->extract_paid_content(),
        _config->attempt_paid_content_json_fixing(), nonce, jsTimeout,
        base::BindOnce(
            callback, weakSelf, annotatedPageContentBarrier, isMainFrame,
            frame->GetSecurityOrigin(),
            DeserializeFrameIdAsLocalFrameToken(frame->GetFrameId())));
  }
}

// Get the WebState's AnnotatedPageContent filled with innerTexts. The
// barrier's callback will be executed for all codepaths in this method.
- (void)processAnnotatedPageContentWithBarrier:(base::RepeatingClosure)barrier {
  if (_shouldGetAnnotatedPageContent) {
    [_pageContextMetrics
        executionStartedForTask:PageContextTask::kAnnotatedPageContent];
  }

  if (_shouldGetInnerText) {
    [_pageContextMetrics executionStartedForTask:PageContextTask::kInnerText];
  }

  web::WebFramesManager* manager = [self webFramesManager];
  std::set<web::WebFrame*> webFrames = manager->GetAllWebFrames();
  web::WebFrame* mainFrame = manager->GetMainWebFrame();

  if (webFrames.empty() || !mainFrame) {
    if (_shouldGetAnnotatedPageContent) {
      [_pageContextMetrics
          executionFinishedForTask:PageContextTask::kAnnotatedPageContent
              withCompletionStatus:PageContextCompletionStatus::kFailure];
    }

    if (_shouldGetInnerText) {
      [_pageContextMetrics
          executionFinishedForTask:PageContextTask::kInnerText
              withCompletionStatus:PageContextCompletionStatus::kFailure];
    }

    barrier.Run();
    return;
  }

  // Create the root node of the APC tree and its first root ContentNode.
  _rootAPCNode =
      std::make_unique<optimization_guide::proto::AnnotatedPageContent>();
  _rootAPCNode->set_version(
      optimization_guide::proto::AnnotatedPageContentVersion::
          ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  _rootAPCNode->mutable_root_node()
      ->mutable_content_attributes()
      ->set_attribute_type(optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  CHECK(_webState);
  _rootAPCNode->set_tab_id(_webState->GetUniqueIdentifier().identifier());

  // Create the aggregated innerText string.
  _innerText = std::make_unique<std::string>();

  // Use a `BarrierClosure` to ensure the JavaScript is done executing in
  // all WebFrames before executing the page context barrier `barrier`,
  // which in turn signals to the PageContextWrapper that the APC is done
  // being processed. The BarrierClosure will wait until the
  // `annotatedPageContentBarrier` callback is itself run once per WebFrame
  // (+1 since we execute the JS explicitly on the main frame first).
  __weak PageContextWrapper* weakSelf = self;
  base::RepeatingClosure annotatedPageContentBarrier = base::BarrierClosure(
      webFrames.size() + 1, base::BindOnce(^{
        [weakSelf handlePageContentExtractionCompletedWithBarrier:barrier];
      }));

  std::string nonce = base::Token::CreateRandom().ToString();

  if (_config->use_refactored_extractor()) {
    // Use the new way for extracting context.

    // Autofill information is only needed when extracting detailed annotated
    // page content. It is not needed when extracting inner text.
    // TODO(crbug.com/493904351): Add kill switch by using autofill config bit.
    if (_shouldGetAnnotatedPageContent) {
      optimization_guide::proto::AutofillInformation* autofillInformation =
          _rootAPCNode->mutable_profile_information()
              ->mutable_autofill_information();
      PopulateAutofillInformation(_webState.get(), autofillInformation);
    }

    if (ios::provider::IsProtectedUrl(mainFrame->GetUrl().spec())) {
      _forceDetachPageContext = YES;
      annotatedPageContentBarrier.Run();
    } else {
      [self extractPageContextForFrame:mainFrame
                           isMainFrame:YES
                                 nonce:nonce
           annotatedPageContentBarrier:annotatedPageContentBarrier];
    }

    // Execute the JavaScript on each other WebFrame and pass in the callback
    // (which executes the barrier when run).
    for (web::WebFrame* webFrame : webFrames) {
      if (ios::provider::IsProtectedUrl(webFrame->GetUrl().spec())) {
        _forceDetachPageContext = YES;
      }

      // Skip if it's the main frame since it was already processed above, or
      // if Page Context should already be force detached.
      if (!webFrame || webFrame->IsMainFrame() || _forceDetachPageContext) {
        annotatedPageContentBarrier.Run();
        continue;
      }

      [self extractPageContextForFrame:webFrame
                           isMainFrame:NO
                                 nonce:nonce
           annotatedPageContentBarrier:annotatedPageContentBarrier];
    }
  } else {
    // Use the legacy way for extracting context.

    // Callback to aggregate values from the JS execution.
    auto callback = [](PageContextWrapper* weakWrapper,
                       base::RepeatingClosure barrier, BOOL isMainFrame,
                       const url::Origin& securityOrigin,
                       std::optional<autofill::LocalFrameToken> frameId,
                       const base::Value* value, NSError* error) {
      [weakWrapper aggregateJavaScriptValue:value
                                  withError:error
                                isMainFrame:isMainFrame
                             securityOrigin:securityOrigin
                            localFrameToken:frameId];
      barrier.Run();
    };

    // Construct the JavaScript script to be executed on each Web Frame with a
    // random token as nonce to differentiate between runs/executions.
    std::u16string script = base::ReplaceStringPlaceholders(
        kInnerTextTreeJavaScript,
        base::span<const std::u16string>(
            {ios::provider::GetPageContextShouldDetachScript(),
             kAnchorTagsJavaScript, base::UTF8ToUTF16(nonce)}),
        nullptr);

    // TODO(crbug.com/452568673): Refactor the force detach logic.

    // If the page is not protected, execute the JavaScript on the main
    // WebFrame first and pass in the callback (which executes the barrier
    // when run).
    if (ios::provider::IsProtectedUrl(mainFrame->GetUrl().spec())) {
      _forceDetachPageContext = YES;
      annotatedPageContentBarrier.Run();
    } else {
      mainFrame->ExecuteJavaScript(
          script,
          base::BindOnce(
              callback, weakSelf, annotatedPageContentBarrier,
              /*isMainFrame=*/YES, mainFrame->GetSecurityOrigin(),
              DeserializeFrameIdAsLocalFrameToken(mainFrame->GetFrameId())));
    }

    // Execute the JavaScript on each other WebFrame and pass in the callback
    // (which executes the barrier when run).
    for (web::WebFrame* webFrame : webFrames) {
      if (ios::provider::IsProtectedUrl(webFrame->GetUrl().spec())) {
        _forceDetachPageContext = YES;
      }

      // Skip if it's the main frame since it was already processed above, or
      // if Page Context should already be force detached.
      if (!webFrame || webFrame->IsMainFrame() || _forceDetachPageContext) {
        annotatedPageContentBarrier.Run();
        continue;
      }

      webFrame->ExecuteJavaScript(
          script,
          base::BindOnce(
              callback, weakSelf, annotatedPageContentBarrier,
              /*isMainFrame=*/NO, webFrame->GetSecurityOrigin(),
              DeserializeFrameIdAsLocalFrameToken(webFrame->GetFrameId())));
    }
  }
}

// All async tasks are complete, execute the overall completion callback.
// Relinquish ownership to the callback handler.
- (void)asyncWorkCompletedForPageContext {
  _timeoutTimer.Stop();

  if (!_completionCallback) {
    return;
  }

  [self stopTextHighlighting];

  PageContextWrapperCallbackResponse response;
  PageContextCompletionStatus completionStatus;

  // Construct the response and completion status, either with the expected
  // value or an error.
  if (!_webState) {
    response = base::unexpected(PageContextWrapperError::kGenericError);
    completionStatus = PageContextCompletionStatus::kFailure;
  } else if (_notExtractable) {
    response =
        base::unexpected(PageContextWrapperError::kPageNotExtractableError);
    completionStatus = PageContextCompletionStatus::kNotExtractable;
  } else if (_forceDetachPageContext) {
    response = base::unexpected(PageContextWrapperError::kForceDetachError);
    completionStatus = PageContextCompletionStatus::kProtected;
  } else if (_shouldGetAnnotatedPageContent &&
             !_pageContext->has_annotated_page_content()) {
    response = base::unexpected(PageContextWrapperError::kAPCError);
    completionStatus = PageContextCompletionStatus::kFailure;
  } else if (_shouldGetInnerText && !_pageContext->has_inner_text()) {
    response = base::unexpected(PageContextWrapperError::kInnerTextError);
    completionStatus = PageContextCompletionStatus::kFailure;
  } else if (_shouldGetFullPagePDF && !_pageContext->has_pdf_data()) {
    response = base::unexpected(PageContextWrapperError::kPDFDataError);
    completionStatus = PageContextCompletionStatus::kFailure;
  } else {
    // TODO(crbug.com/483989948): Make screenshot failure blocking once
    // 'aw, snap' snackbar is fixed.

    // TODO(crbug.com/509589346): CHECK `registrar` since it is known here that
    // there is a `_webState`.
    // Set the focused frame based on the collected data on the same origin and
    // cross-origin frames.
    autofill::ChildFrameRegistrar* registrar = [self frameRegistrar];
    if (_config->use_rich_extraction() && registrar) {
      ResolveFocusedFrame(_focusedFrameInfos, _grafter.GetRemoteFrames(),
                          registrar,
                          _pageContext->mutable_annotated_page_content());
    }

    if (_config->graft_cross_origin_frame_content() && registrar) {
      ResolveCrossSiteFrameContent(
          _grafter, registrar, _pageContext->mutable_annotated_page_content());
    }
    response = base::ok(std::move(_pageContext));
    completionStatus = PageContextCompletionStatus::kSuccess;
  }

  [_pageContextMetrics executionFinishedForTask:PageContextTask::kOverall
                           withCompletionStatus:completionStatus];

  std::move(_completionCallback).Run(std::move(response));
}

// Returns YES if the image is nil and forcing the update of missing snapshots
// is enabled.
- (BOOL)shouldUpdateSnapshotWithImage:(UIImage*)image {
  return !image && _shouldForceUpdateMissingSnapshots;
}

// Updates the snapshot for the given WebState, and executes the `barrier`
// callback when finished.
- (void)updateSnapshotWithBarrier:(base::RepeatingClosure)barrier {
  if (!_webState) {
    barrier.Run();
    return;
  }
  __weak PageContextWrapper* weakSelf = self;
  SnapshotTabHelper::FromWebState(_webState.get())
      ->UpdateSnapshotWithCallback(^(UIImage* image) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf encodeImageAndSetTabScreenshot:image];
        barrier.Run();
      });
}

// Updates the current WebState's snapshot with the given callback.
- (void)updateSnapshotWithCallback:(void (^)(UIImage*))callback {
  if (_webState) {
    SnapshotTabHelper::FromWebState(_webState.get())
        ->UpdateSnapshotWithCallback(callback);
  } else {
    callback(nil);
  }
}

// Convert UIImage snapshot to PNG, and then to base64 encoded string. Set the
// tab screenshot on the current PageContext.
- (void)encodeImageAndSetTabScreenshot:(UIImage*)image {
  [self stopTextHighlighting];

  if (!image) {
    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kScreenshot
            withCompletionStatus:PageContextCompletionStatus::kFailure];
    DLOG(WARNING) << "Failed to fetch webpage screenshot.";
    return;
  }

  NSData* imageData = UIImagePNGRepresentation(image);
  if (!imageData) {
    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kScreenshot
            withCompletionStatus:PageContextCompletionStatus::kFailure];
    DLOG(WARNING) << "Failed to convert the screenshot to PNG.";
    return;
  }

  NSString* base64String = [imageData base64EncodedStringWithOptions:0];
  _pageContext->set_tab_screenshot(base::SysNSStringToUTF8(base64String));

  [_pageContextMetrics
      executionFinishedForTask:PageContextTask::kScreenshot
          withCompletionStatus:PageContextCompletionStatus::kSuccess];
}

// If it exists, convert the PDF data to base64 encoded string and set it in
// the PageContext proto.
- (void)encodeAndSetFullPagePDF:(NSData*)PDFData {
  if (!PDFData) {
    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kPDF
            withCompletionStatus:PageContextCompletionStatus::kFailure];
    DLOG(WARNING) << "Failed to fetch webpage PDF data.";
    return;
  }

  NSString* base64String = [PDFData base64EncodedStringWithOptions:0];
  _pageContext->set_pdf_data(base::SysNSStringToUTF8(base64String));

  [_pageContextMetrics
      executionFinishedForTask:PageContextTask::kPDF
          withCompletionStatus:PageContextCompletionStatus::kSuccess];
}

// Adds a frame's focus info to the flat array if it is focused.
- (void)addFrameFocusInfo:(bool)isFocused
               documentId:(const std::string&)documentId {
  if (isFocused) {
    FrameFocusInfo info;
    info.document_id = documentId;
    _focusedFrameInfos.push_back(std::move(info));
  }
}

// Helper to populate the Rich Extraction content for both the main frame and
// iframes.
- (void)populateForRichExtractionWithValue:(const base::DictValue&)value
                            securityOrigin:(const url::Origin&)securityOrigin
                               isMainFrame:(BOOL)isMainFrame
                           localFrameToken:
                               (std::optional<autofill::LocalFrameToken>)
                                   localFrameToken {
  // Populate the annotated page content for main frame including
  // frame data.
  const base::DictValue* rootNodeValue = value.FindDict("rootNode");
  if (!rootNodeValue) {
    // There is no point in processing content if there is no root node.
    return;
  }
  const base::DictValue* frameDataValue = value.FindDict("frameData");

  optimization_guide::proto::ContentNode* destinationContentNode = nullptr;
  optimization_guide::proto::FrameData* destinationFrameData = nullptr;

  // Pick the destinations for the APC node content and frame data.
  if (isMainFrame) {
    // Main frame: Use the root node as the destination.
    destinationContentNode = _rootAPCNode->mutable_root_node();
    destinationFrameData = _rootAPCNode->mutable_main_frame_data();

    // Populate DocumentIdentifier for the Main Frame. It is not provided from
    // the renderer so we need to do it here.
    autofill::RemoteFrameToken token = static_cast<autofill::RemoteFrameToken>(
        base::UnguessableToken::Create());
    destinationFrameData->mutable_document_identifier()->set_serialized_token(
        token.ToString());

    // Register the Main Frame token to allow lookups (e.g. for actions).
    autofill::ChildFrameRegistrar* registrar = [self frameRegistrar];
    if (registrar && localFrameToken) {
      registrar->RegisterMapping(token, *localFrameToken);
    }

  } else if (localFrameToken) {
    // Grafting possible: Use the content node directly from the grafter.
    FrameGrafter::FrameContent* content =
        _grafter.DeclareContent(*localFrameToken);
    if (!content) {
      // Content already declared or invalid, skip processing.
      return;
    }
    destinationContentNode = &content->content;
    destinationFrameData = &content->frame_data;
  } else {
    // Grafting not possible: Add new child to the root node as the default
    // location for the iframe node. Set that node to be an iframe node.
    destinationContentNode =
        _rootAPCNode->mutable_root_node()->add_children_nodes();
    destinationContentNode->mutable_content_attributes()->set_attribute_type(
        optimization_guide::proto::ContentAttributeType::
            CONTENT_ATTRIBUTE_IFRAME);
    destinationFrameData = destinationContentNode->mutable_content_attributes()
                               ->mutable_iframe_data()
                               ->mutable_frame_data();
  }

  // Destination placeholders must be set to something even if their content
  // won't be set.
  CHECK(destinationContentNode);
  CHECK(destinationFrameData);

  // Having root node content is a must at this point.
  CHECK(rootNodeValue);

  // Callback to collect the focus status of each traversed frame (same-origin
  // nested frames and the current frame) into the flat array.
  __weak __typeof(self) weakSelf = self;
  base::RepeatingCallback<void(bool is_focused, const std::string& document_id)>
      on_frame_extracted = base::BindRepeating(
          ^(bool isFocusedChild, const std::string& documentId) {
            [weakSelf addFrameFocusInfo:isFocusedChild documentId:documentId];
          });

  std::optional<AutofillExtractionContext> autofill_context;
  if (_config->extract_autofill()) {
    autofill_context.emplace(_webState, localFrameToken,
                             _config->extract_autofill_credit_card_redactions(),
                             &_autofillSectionNumbers);
  }

  PopulateAPCNodeFromContentTree(
      *rootNodeValue, securityOrigin, _grafter,
      autofill_context ? &*autofill_context : nullptr, destinationContentNode,
      on_frame_extracted);

  // Populate the frame data for this frame and determine whether it is focused.
  bool isFocused = false;
  if (frameDataValue) {
    isFocused = PopulateFrameDataNode(*frameDataValue, securityOrigin,
                                      destinationFrameData)
                    .is_focused;
  }

  // Record focus status for later resolution if focused unless the frame is
  // the main frame which can be resolved right away.
  if (isFocused) {
    FrameFocusInfo info;
    info.local_token = localFrameToken;
    if (isMainFrame) {
      info.document_id =
          destinationFrameData->document_identifier().serialized_token();
    }
    _focusedFrameInfos.push_back(std::move(info));
  }

  // Populate the page data extracted from the main frame.
  if (isMainFrame) {
    const base::DictValue* pageInteractionInfoValue =
        value.FindDict(kPageInteractionInfoDictKey);
    if (pageInteractionInfoValue) {
      PopulatePageInteractionInfoNode(
          *pageInteractionInfoValue,
          _rootAPCNode->mutable_page_interaction_info());
    }

    if (const base::DictValue* viewportGeometryValue =
            value.FindDict(kViewportGeometryDictKey)) {
      PopulateViewportGeometryNode(*viewportGeometryValue,
                                   _rootAPCNode->mutable_viewport_geometry());
    }
  }
}

// Helper to populate the main frame's content for light extraction.
- (void)populateMainFrameForLightExtractionWithValue:
            (const base::DictValue&)value
                                      securityOrigin:
                                          (const url::Origin&)securityOrigin {
  // Light Extraction: Populate the annotated page content for main frame.
  [self populateMainFrameSubtreeForLightExtractionWithValue:value
                                                     origin:securityOrigin];

  // Recursively populate the ContentNode subtree
  // for any of the main WebFrame's children iframes. Children can be remote
  //  or same origin frames.
  const base::ListValue* childrenFrames =
      value.FindList(kChildrenFramesDictKey);
  if (childrenFrames && !childrenFrames->empty()) {
    for (const auto& childFrame : *childrenFrames) {
      if (!childFrame.is_dict()) {
        continue;
      }
      // Note: We need `node` to hold until extraction is done because it
      // is part of the root APC node content.
      optimization_guide::proto::ContentNode* node =
          _rootAPCNode->mutable_root_node()->add_children_nodes();
      [self populateIframeSubtreeWithValue:childFrame.GetDict()
                                    origin:securityOrigin
                                      node:node];
    }
  }
}

// Helper to populate an iframe's content for light extraction.
- (void)populateIframeForLightExtractionWithValue:(const base::DictValue&)value
                                   securityOrigin:
                                       (const url::Origin&)securityOrigin
                                  localFrameToken:
                                      (std::optional<autofill::LocalFrameToken>)
                                          localFrameToken {
  optimization_guide::proto::ContentNode* destinationContentNode;

  // Pick a destination ContentNode to fill with the iframe content.
  if (_config->graft_cross_origin_frame_content() && localFrameToken) {
    // Grafting possible: Populate iframe content by respecting the DOM
    // structure.

    // In ApcV1, all the frame data is stored in the `content` field.
    destinationContentNode =
        &_grafter.DeclareContent(*localFrameToken)->content;
  } else {
    // Grafting not possible: Add new child to the root node as the default
    // location for the iframe node.
    destinationContentNode =
        _rootAPCNode->mutable_root_node()->add_children_nodes();
  }

  CHECK(destinationContentNode);

  [self populateIframeSubtreeWithValue:value
                                origin:securityOrigin
                                  node:destinationContentNode];
}

// If it exists, parse the returned JavaScript value from the WebFrame,
// construct its ContentNode subtree and insert it into the APC tree.
- (void)aggregateJavaScriptValue:(const base::Value*)value
                       withError:(NSError*)error
                     isMainFrame:(BOOL)isMainFrame
                  securityOrigin:(const url::Origin&)securityOrigin
                 localFrameToken:
                     (std::optional<autofill::LocalFrameToken>)localFrameToken {
  if (error || !value || !value->is_dict() || !_webState) {
    if (error) {
      // TODO(crbug.com/401282824): Log the failure rate of aggregation.
      DLOG(WARNING) << "Failed to fetch frame's innerText tree."
                    << base::SysNSStringToUTF8([error localizedDescription]);
    }
    return;
  }

  if (_forceDetachPageContext) {
    return;
  }

  const base::DictValue& valueAsDict = value->GetDict();

  // Check if PageContext should be force detached.
  // TODO(crbug.com/471244309): Force detaching PageContext shouldn't depend on
  // fetching innerText/APC, it should always be enabled.
  std::optional<bool> shouldDetachPageContext =
      valueAsDict.FindBool(kShouldDetachPageContext);
  if (shouldDetachPageContext.has_value() && shouldDetachPageContext.value()) {
    _forceDetachPageContext = YES;

    if (_shouldGetAnnotatedPageContent) {
      [_pageContextMetrics
          executionFinishedForTask:PageContextTask::kAnnotatedPageContent
              withCompletionStatus:PageContextCompletionStatus::kProtected];
    }

    if (_shouldGetInnerText) {
      [_pageContextMetrics
          executionFinishedForTask:PageContextTask::kInnerText
              withCompletionStatus:PageContextCompletionStatus::kProtected];
    }

    return;
  }

  if (_config->graft_cross_origin_frame_content() &&
      valueAsDict.FindString(kRemoteFrameTokenKey)) {
    // Do not aggregate the results if they contain a remote frame token as the
    // root content from frame extraction which is unexpected and shouldn't be
    // handled.
    // TODO(crbug.com/464473686): Measure this via a metric.
    return;
  }

  // Create a special subtree for the mainframe, and then recursively populate
  // its children iframe subtrees. Else, recursively populate cross-origin
  // iframes.
  if (isMainFrame) {
    // Populate main frame root node.

    if (_config->use_rich_extraction()) {
      [self populateForRichExtractionWithValue:valueAsDict
                                securityOrigin:securityOrigin
                                   isMainFrame:YES
                               localFrameToken:localFrameToken];
    } else {
      [self populateMainFrameForLightExtractionWithValue:valueAsDict
                                          securityOrigin:securityOrigin];
    }
    return;
  }

  // Populate iframes nodes from the root.

  if (_config->use_rich_extraction()) {
    [self populateForRichExtractionWithValue:valueAsDict
                              securityOrigin:securityOrigin
                                 isMainFrame:NO
                             localFrameToken:localFrameToken];
  } else {
    [self populateIframeForLightExtractionWithValue:valueAsDict
                                     securityOrigin:securityOrigin
                                    localFrameToken:localFrameToken];
  }
}

// Called when all JS extraction tasks are completed. It handles the remote
// frame registration waiting logic before finalizing the APC tree.
- (void)handlePageContentExtractionCompletedWithBarrier:
    (base::RepeatingClosure)barrier {
  [self webFramesAnnotatedPageContentFetchCompleted];

  if (!_config->graft_cross_origin_frame_content()) {
    barrier.Run();
    return;
  }

  std::vector<autofill::RemoteFrameToken> discoveredRemoteTokens =
      _grafter.GetRemoteFrames();
  if (discoveredRemoteTokens.empty()) {
    barrier.Run();
    return;
  }

  // TODO(crbug.com/475858687): Measure registration time.

  autofill::ChildFrameRegistrar* registrar = [self frameRegistrar];
  if (!registrar) {
    barrier.Run();
    return;
  }

  // Reset the flag for the new wait cycle.
  _registrationCompletedOrTimedOut = NO;

  // TODO(crbug.com/475854782): Measure registration timeouts.
  __weak PageContextWrapper* weakSelf = self;
  auto run_barrier_once = base::BindRepeating(
      [](base::RepeatingClosure barrier, __weak PageContextWrapper* weakSelf) {
        [weakSelf completeRegistrationWaitWithBarrier:barrier];
      },
      std::move(barrier), weakSelf);

  // Start the global registration timer.
  _registrationTimeoutTimer.Start(FROM_HERE, kRegistrationTimeout,
                                  run_barrier_once);

  // The barrier that waits for all registrations.
  base::RepeatingClosure registrationBarrier =
      base::BarrierClosure(discoveredRemoteTokens.size(), run_barrier_once);

  for (const auto& token : discoveredRemoteTokens) {
    registrar->DeclareNewRemoteToken(
        token, base::BindOnce([](base::RepeatingClosure barrier,
                                 autofill::LocalFrameToken) { barrier.Run(); },
                              registrationBarrier));
  }
}

// Set the constructed APC tree on the PageContext proto.
- (void)webFramesAnnotatedPageContentFetchCompleted {
  if (_shouldGetInnerText) {
    _pageContext->set_allocated_inner_text(_innerText.release());

    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kInnerText
            withCompletionStatus:PageContextCompletionStatus::kSuccess];
  }

  if (_shouldGetAnnotatedPageContent) {
    size_t sizeInBytes = _rootAPCNode->ByteSizeLong();

    _pageContext->set_allocated_annotated_page_content(_rootAPCNode.release());

    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kAnnotatedPageContent
            withCompletionStatus:PageContextCompletionStatus::kSuccess];
    [_pageContextMetrics logAnnotatedPageContentSize:sizeInBytes];
  }
}

// Populate the main frame's ContentNode subtree with the correct nodes and
// their values. Adds Main Frame data and the root text ContentNode.
- (void)populateMainFrameSubtreeForLightExtractionWithValue:
            (const base::DictValue&)value
                                                     origin:(const url::Origin&)
                                                                origin {
  // Set the main frame's security origin.
  PopulateFrameDataNode(value, origin, _rootAPCNode->mutable_main_frame_data());

  // Set its child text node.
  [self populateTextInfoNodeWithValue:value
                               origin:origin
                           parentNode:_rootAPCNode->mutable_root_node()];

  // Set its children anchor nodes.
  [self populateAnchorNodeChildrenWithValue:value
                                 parentNode:_rootAPCNode->mutable_root_node()];
}

// Populate a ContentNode with a TextInfo node and its correct values.
- (void)populateTextInfoNodeWithValue:(const base::DictValue&)value
                               origin:(const url::Origin&)origin
                           parentNode:(optimization_guide::proto::ContentNode*)
                                          parentNode {
  if (!parentNode) {
    return;
  }

  // Early return if there is no text to add.
  const std::string* innerTextPtr =
      value.FindString(kCurrentNodeInnerTextDictKey);
  if (!innerTextPtr) {
    return;
  }
  std::string_view trimmedText =
      base::TrimWhitespaceASCII(*innerTextPtr, base::TRIM_ALL);
  if (trimmedText.empty()) {
    return;
  }

  // Create and add the text node.
  optimization_guide::proto::ContentNode* childTextNode =
      parentNode->add_children_nodes();
  childTextNode->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  childTextNode->mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content(trimmedText);

  if (_shouldGetInnerText) {
    _innerText->append(trimmedText);
  }
}

// Populate the ContentNode subtree for an iframe with the correct values.
// Also recursively populates the subtrees for all of this iframe's children.
- (void)populateIframeSubtreeWithValue:(const base::DictValue&)value
                                origin:(const url::Origin&)origin
                                  node:(optimization_guide::proto::ContentNode*)
                                           node {
  const std::string* remoteTokenString =
      _config->graft_cross_origin_frame_content()
          ? value.FindString(kRemoteFrameTokenKey)
          : nullptr;

  if (remoteTokenString) {
    // Register a placeholder for the frame content when the frame is on a
    // different origin from the parent frame.
    //
    // TODO(crbug.com/460823916): The grafter should re-attempt registering the
    // placeholder upon registration completion via DeclareNewRemoteToken(). We
    // could use extra barriers for that.
    std::optional<autofill::RemoteFrameToken> remote =
        DeserializeFrameIdAsRemoteFrameToken(*remoteTokenString);
    if (remote) {
      _grafter.RegisterPlaceholder(*remote, node);
    }

    return;
  }

  // Create the child iframe node.
  node->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  // Set its FrameData values.
  optimization_guide::proto::FrameData* nodeFrameData =
      node->mutable_content_attributes()
          ->mutable_iframe_data()
          ->mutable_frame_data();
  PopulateFrameDataNode(value, origin, nodeFrameData);

  // Create the nested root child ContentNode.
  optimization_guide::proto::ContentNode* childRootNode =
      node->add_children_nodes();
  childRootNode->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  // Create the nested text node.
  [self populateTextInfoNodeWithValue:value
                               origin:origin
                           parentNode:childRootNode];

  // Create the children anchor nodes.
  [self populateAnchorNodeChildrenWithValue:value parentNode:childRootNode];

  // Recursively populate the ContentNode subtree for any children iframes.
  // Child frame content will either be filled immediately or claimed for
  // later.
  const base::ListValue* childrenFrames =
      value.FindList(kChildrenFramesDictKey);
  if (childrenFrames && !childrenFrames->empty()) {
    for (const auto& childFrame : *childrenFrames) {
      if (childFrame.is_dict()) {
        optimization_guide::proto::ContentNode* childNode =
            childRootNode->add_children_nodes();
        [self populateIframeSubtreeWithValue:childFrame.GetDict()
                                      origin:origin
                                        node:childNode];
      }
    }
  }
  return;
}

// Populate all anchor tags as AnchorData nodes which are direct children
// of `parentNode`.
- (void)populateAnchorNodeChildrenWithValue:(const base::DictValue&)value
                                 parentNode:
                                     (optimization_guide::proto::ContentNode*)
                                         parentNode {
  if (!parentNode) {
    return;
  }

  const base::ListValue* links = value.FindList(kFrameLinksDictKey);
  if (!links || links->empty()) {
    return;
  }

  for (const auto& linkValue : *links) {
    [self populateAnchorNodeWithValue:&linkValue parentNode:parentNode];
  }
}

// Creates an AnchorData node (with the corresponding URL) with one child
// TextInfo node (with the corresponding innerText). Set the AnchorData node
// as direct child of `parentNode`.
- (void)populateAnchorNodeWithValue:(const base::Value*)linkData
                         parentNode:(optimization_guide::proto::ContentNode*)
                                        parentNode {
  if (!linkData || !linkData->is_dict() || !parentNode) {
    return;
  }

  const std::string* href = linkData->GetDict().FindString(kLinkHREFDictKey);
  if (!href || href->empty()) {
    return;
  }

  // Create the anchor node.
  optimization_guide::proto::ContentNode* anchorNode =
      parentNode->add_children_nodes();
  anchorNode->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR);

  // Set the anchor data (the HREF).
  anchorNode->mutable_content_attributes()->mutable_anchor_data()->set_url(
      *href);

  // Create a child text node for the anchor's innerText.
  const std::string* linkText =
      linkData->GetDict().FindString(kLinkTextDictKey);
  if (!linkText || linkText->empty() ||
      base::TrimWhitespaceASCII(*linkText, base::TRIM_ALL).empty()) {
    return;
  }

  // Set the child text node's text value.
  optimization_guide::proto::ContentNode* textNode =
      anchorNode->add_children_nodes();
  textNode->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  textNode->mutable_content_attributes()->mutable_text_data()->set_text_content(
      *linkText);
}

// Stop the highlighting of text.
- (void)stopTextHighlighting {
  if (!_textToHighlight) {
    return;
  }

  web::WebState* webState = _webState.get();
  if (!webState) {
    return;
  }

  web::FindInPageJavaScriptFeature* find_in_page_feature =
      web::FindInPageJavaScriptFeature::GetInstance();
  if (!find_in_page_feature) {
    return;
  }

  web::WebFramesManager* framesManager = webState->GetWebFramesManager(
      find_in_page_feature->GetSupportedContentWorld());
  if (!framesManager) {
    return;
  }

  web::WebFrame* mainFrame = framesManager->GetMainWebFrame();

  if (!mainFrame) {
    return;
  }

  find_in_page_feature->Stop(mainFrame);
}

// Called when the overall execution times out. Cancels the timer and executes
// the completion callback with `kTimeout`.
- (void)onTimeout {
  if (!_completionCallback) {
    return;
  }

  [self stopTextHighlighting];

  DLOG(WARNING) << "PageContextWrapper execution timed out.";

  [_pageContextMetrics
      executionFinishedForTask:PageContextTask::kOverall
          withCompletionStatus:PageContextCompletionStatus::kTimeout];

  std::move(_completionCallback)
      .Run(base::unexpected(PageContextWrapperError::kTimeout));
}

@end
