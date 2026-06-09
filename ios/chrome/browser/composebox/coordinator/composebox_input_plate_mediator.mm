// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_mediator.h"

#import <PDFKit/PDFKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <memory>
#import <optional>
#import <queue>
#import <set>
#import <unordered_map>
#import <unordered_set>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/debug/dump_without_crashing.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted_memory.h"
#import "base/sequence_checker.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "components/contextual_search/contextual_search_context_controller.h"
#import "components/contextual_search/contextual_search_metrics_recorder.h"
#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/contextual_search/input_state_model.h"
#import "components/contextual_search/internal/ios/composebox_context_upload_observer_bridge.h"
#import "components/contextual_search/internal/ios/composebox_query_controller_ios.h"
#import "components/contextual_tasks/public/query_contextualizer.h"
#import "components/lens/contextual_input.h"
#import "components/lens/lens_bitmap_processing.h"
#import "components/lens/lens_overlay_mime_type.h"
#import "components/lens/lens_url_utils.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/browser/lens_suggest_inputs_utils.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/omnibox/composebox/composebox_query.mojom.h"
#import "components/omnibox/composebox/contextual_search_mojom_traits.h"
#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/cobrowse/model/ios_contextual_tasks_service_factory.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_query_contextualizer_delegate_bridge.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_url_loader.h"
#import "ios/chrome/browser/composebox/debugger/composebox_debugger_logger.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_selection.h"
#import "ios/chrome/browser/composebox/public/composebox_constants.h"
#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"
#import "ios/chrome/browser/composebox/public/composebox_input_plate_controls.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_attachment_diff.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_drive_result.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_collection.h"
#import "ios/chrome/browser/composebox/ui/composebox_strings.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/model/utils/web_state_deferred_executor.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "mojo/public/cpp/base/big_buffer.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#import "third_party/omnibox_proto/model_config.pb.h"
#import "third_party/omnibox_proto/model_mode.pb.h"
#import "third_party/omnibox_proto/searchbox_config.pb.h"
#import "third_party/omnibox_proto/tool_config.pb.h"
#import "third_party/omnibox_proto/tool_mode.pb.h"
#import "ui/base/page_transition_types.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

namespace {

// The timeout for Page Context execution.
constexpr base::TimeDelta kPageContextTimeout = base::Seconds(10);

// Reads data from a file URL. Runs on a background thread.
NSData* ReadDataFromURL(GURL url) {
  NSURL* ns_url = net::NSURLWithGURL(url);
  BOOL accessing = [ns_url startAccessingSecurityScopedResource];

  NSData* data = nil;
  @try {
    // Always attempt to read the data. This will work for non-scoped URLs,
    // and for scoped URLs if `accessing` is true. It will fail if `accessing`
    // is false for a scoped URL, which is the correct behavior.
    data = [NSData dataWithContentsOfURL:ns_url];
  } @finally {
    // Only stop accessing if we were successfully granted access.
    if (accessing) {
      [ns_url stopAccessingSecurityScopedResource];
    }
  }
  return data;
}

// Generates a UIImage preview for the given PDF data.
UIImage* GeneratePDFPreview(NSData* pdf_data) {
  if (!pdf_data) {
    return nil;
  }
  PDFDocument* doc = [[PDFDocument alloc] initWithData:pdf_data];
  if (!doc) {
    return nil;
  }
  PDFPage* page = [doc pageAtIndex:0];
  if (!page) {
    return nil;
  }
  // TODO(crbug.com/40280872): Determine the correct size for the thumbnail.
  return [page thumbnailOfSize:CGSizeMake(200, 200)
                        forBox:kPDFDisplayBoxCropBox];
}

// Creates an initial ContextualInputData object using the information from the
// passed in `annotated_page_content` and `web_state`.
std::unique_ptr<lens::ContextualInputData>
CreateInputDataFromAnnotatedPageContent(
    std::unique_ptr<optimization_guide::proto::AnnotatedPageContent>
        annotated_page_content,
    web::WebState* web_state) {
  if (!annotated_page_content || !web_state) {
    return nullptr;
  }

  std::string serialized_context;
  annotated_page_content->SerializeToString(&serialized_context);

  std::vector<uint8_t> vector_data(serialized_context.begin(),
                                   serialized_context.end());

  auto input_data = std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kAnnotatedPageContent;
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->context_input->push_back(lens::ContextualInput(
      std::move(vector_data), lens::MimeType::kAnnotatedPageContent));

  input_data->page_url = web_state->GetVisibleURL();
  input_data->page_title = base::UTF16ToUTF8(web_state->GetTitle());
  return input_data;
}

// Helper to map ComposeboxInputItemCollection to lens::MimeType
std::vector<lens::MimeType> MimeTypesFromCollection(
    ComposeboxInputItemCollection* items) {
  std::vector<lens::MimeType> types;
  if (!items || items.empty) {
    return types;
  }

  if (items.imagesCount > 0) {
    types.push_back(lens::MimeType::kImage);
  }
  if (items.tabsCount > 0) {
    types.push_back(lens::MimeType::kAnnotatedPageContent);
  }
  if (items.filesCount > 0) {
    types.push_back(lens::MimeType::kPdf);
  }

  return types;
}

// Returns the default image encoding options.
// TODO(crbug.com/40280872): Plumb encoding options from a central config.
lens::ImageEncodingOptions GetDefaultImageEncodingOptions() {
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;
  return image_options;
}

}  // namespace

@interface ComposeboxInputPlateMediator () <
    SearchEngineObserving,
    ComposeboxInputItemCollectionDelegate,
    WebStateDeferredExecutorDelegate,
    ComposeboxQueryContextualizerDelegate>

@end


@implementation ComposeboxInputPlateMediator {
  // The ordered list of items for display.
  ComposeboxInputItemCollection* _items;
  // Whether we are awaiting attachment signals to be ready in order to show
  // suggestions. This is used during the initial focus flow to skip the
  // zero-suggest requests, to avoid flashing the suggestions.
  BOOL _awaitingAttachmentSignals;
  // The C++ session handle for this feature.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      _contextualSearchSession;
  // The observer bridge for context upload status.
  std::unique_ptr<ComposeboxContextUploadObserverBridge>
      _composeboxObserverBridge;
  // The different modes for the composebox.
  ComposeboxModeHolder* _modeHolder;
  // The web state list.
  raw_ptr<WebStateList> _webStateList;
  // The favicon loader.
  raw_ptr<FaviconLoader> _faviconLoader;
  // A browser agent for retrieving APC from the cache.
  raw_ptr<PersistTabContextBrowserAgent> _persistTabContextAgent;
  // A template URL service.
  raw_ptr<TemplateURLService> _templateURLService;
  // Observer for the TemplateURLService.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Service to check for AI mode eligibility.
  raw_ptr<AimEligibilityService> _aimEligibilityService;
  // The preference service.
  raw_ptr<PrefService> _prefService;
  // The profile.
  raw_ptr<ProfileIOS> _profile;
  // Browser agent to manage the cobrowse context.
  raw_ptr<CobrowseBrowserAgent> _cobrowseBrowserAgent;

  // Cached current tab favicon.
  UIImage* _currentTabFavicon;

  // Stores the page context wrappers for the duration of the APC retrieval.
  std::unordered_map<web::WebStateID, PageContextWrapper*> _pageContextWrappers;
  std::unordered_map<base::UnguessableToken,
                     web::WebStateID,
                     base::UnguessableTokenHash>
      _latestTabSelectionMapping;

  // Delegate for the query contextualizer.
  std::unique_ptr<QueryContextualizerDelegateBridge>
      _queryContextualizerDelegate;
  // Orchestrator to contextualize tabs before search.
  std::unique_ptr<contextual_tasks::QueryContextualizer> _queryContextualizer;

  // Utilitary to delay execution until the web state is loaded.
  WebStateDeferredExecutor* _webStateDeferredExecutor;

  // Check that the different methods are called from the correct sequence, as
  // this class defers work via PostTask APIs.
  SEQUENCE_CHECKER(_sequenceChecker);

  // Whether the textfield is multiline or not.
  BOOL _isMultiline;
  // Whether the browser is in incognito mode.
  BOOL _isIncognito;
  // Whether the mediator is currently updating the compact mode.
  BOOL _isUpdatingCompactMode;
  // Caches whether user input is in progress.
  BOOL _userInputInProgress;
  // Caches whether the current input is a search query.
  BOOL _isSearchQuery;
  // Whether it is in compact mode.
  BOOL _compact;
  // Whether the omnibox has text inputted.
  BOOL _hasText;
  // Whether a successful navigation has started.
  BOOL _inNavigation;
  // Whether the omnibox is focused.
  BOOL _omniboxFocused;
  // Used to count the number of images added in the session.
  int _imageUploadCount;

  // The state reflecting the availbale modes and models.
  ComposeboxInputStateManager* _stateManager;
  // Handler for browser coordinator commands.
  __weak id<BrowserCoordinatorCommands> _browserCoordinatorHandler;
  // Handler for scene commands.
  __weak id<SceneCommands> _sceneHandler;
  // The entrypoint from which the composebox was invoked.
  ComposeboxEntrypoint _entrypoint;
  // The previously observed mode of the composebox.
  ComposeboxMode _previousMode;
}

- (void)setMetricsRecorder:(ComposeboxMetricsRecorder*)metricsRecorder {
  _metricsRecorder = metricsRecorder;
  _stateManager.metricsRecorder = metricsRecorder;
}

- (instancetype)
    initWithContextualSearchSession:
        (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)
            contextualSearchSession
                       webStateList:(WebStateList*)webStateList
                      faviconLoader:(FaviconLoader*)faviconLoader
             persistTabContextAgent:
                 (PersistTabContextBrowserAgent*)persistTabContextAgent
                        isIncognito:(BOOL)isIncognito
                         modeHolder:(ComposeboxModeHolder*)modeHolder
                 templateURLService:(TemplateURLService*)templateURLService
              aimEligibilityService:
                  (AimEligibilityService*)aimEligibilityService
                        prefService:(PrefService*)prefService
                            profile:(ProfileIOS*)profile
               cobrowseBrowserAgent:(CobrowseBrowserAgent*)cobrowseBrowserAgent
          browserCoordinatorHandler:
              (id<BrowserCoordinatorCommands>)browserCoordinatorHandler
                       sceneHandler:(id<SceneCommands>)sceneHandler
                         entrypoint:(ComposeboxEntrypoint)entrypoint {
  self = [super init];
  if (self) {
    _entrypoint = entrypoint;
    _browserCoordinatorHandler = browserCoordinatorHandler;
    _sceneHandler = sceneHandler;
    _prefService = prefService;
    _profile = profile;
    _cobrowseBrowserAgent = cobrowseBrowserAgent;
    _contextualSearchSession = std::move(contextualSearchSession);
    if (_contextualSearchSession) {
      _contextualSearchSession->NotifySessionStarted();
      CHECK(_contextualSearchSession->GetController());
      _composeboxObserverBridge =
          std::make_unique<ComposeboxContextUploadObserverBridge>(
              self, _contextualSearchSession->GetController());
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(appDidEnterBackground:)
                 name:UIApplicationDidEnterBackgroundNotification
               object:nil];
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(appWillEnterForeground:)
                 name:UIApplicationWillEnterForegroundNotification
               object:nil];
    }
    _webStateList = webStateList;
    _faviconLoader = faviconLoader;
    _webStateDeferredExecutor = [[WebStateDeferredExecutor alloc] init];
    _webStateDeferredExecutor.delegate = self;
    _persistTabContextAgent = persistTabContextAgent;
    _isIncognito = isIncognito;
    _modeHolder = modeHolder;
    _previousMode = _modeHolder.mode;
    _templateURLService = templateURLService;
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _aimEligibilityService = aimEligibilityService;
    _items = [[ComposeboxInputItemCollection alloc] init];
    _items.delegate = self;

    signin::IdentityManager* identityManager =
        _profile ? IdentityManagerFactory::GetForProfile(_profile) : nullptr;
    _stateManager = [[ComposeboxInputStateManager alloc]
         initWithWebStateList:_webStateList
                   modeHolder:_modeHolder
                  prefService:_prefService
        aimEligibilityService:_aimEligibilityService
              identityManager:identityManager
           templateURLService:_templateURLService
                sessionHandle:_contextualSearchSession.get()
                   entrypoint:_entrypoint
                  isIncognito:_isIncognito];
    _stateManager.delegate = self;
    _stateManager.items = _items;

    contextual_tasks::ContextualTasksService* tasksService =
        IOSContextualTasksServiceFactory::GetForProfile(_profile);
    if (tasksService) {
      _queryContextualizerDelegate =
          std::make_unique<QueryContextualizerDelegateBridge>(self);
      _queryContextualizer =
          std::make_unique<contextual_tasks::QueryContextualizer>(
              tasksService, _queryContextualizerDelegate.get());
    }

    if (_entrypoint == ComposeboxEntrypoint::kCobrowse) {
      CHECK([_stateManager isEligibleToAIM])
          << "The Cobrowse entry point requires AIM eligibility. Accessing it "
             "without valid eligibility represents an illegal state.";
    }
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self recordSessionEndMetrics];
  _modeHolder = nil;
  _faviconLoader = nullptr;
  _webStateDeferredExecutor = nil;
  _persistTabContextAgent = nullptr;
  _searchEngineObserver.reset();
  _templateURLService = nullptr;
  [_stateManager disconnect];
  _stateManager = nil;
  _aimEligibilityService = nullptr;
  _cobrowseBrowserAgent = nullptr;
  _queryContextualizer.reset();
  _queryContextualizerDelegate.reset();
  _composeboxObserverBridge.reset();
  if (_contextualSearchSession) {
    if (!_inNavigation) {
      _contextualSearchSession->NotifySessionAbandoned();
    }
    _contextualSearchSession.reset();
  }
  _inNavigation = NO;
  _webStateList = nil;
  _items = nil;
  _URLLoader = nil;
  _consumer = nil;
  _prefService = nullptr;
  _profile = nullptr;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)setConsumer:(id<ComposeboxInputPlateConsumer>)consumer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _consumer = consumer;

  if (!_webStateList || !consumer) {
    return;
  }

  if ([self canAttachActiveTab]) {
    [self extractFaviconForCurrentTab];
  }

  if (self.isCobrowse) {
    _modeHolder.mode = ComposeboxMode::kAIM;
  }

  [self commitUIUpdates];
}

- (BOOL)canAddMoreAttachments {
  return [_stateManager canAddMoreAttachments];
}

- (NSUInteger)remainingAttachmentCapacity {
  return [_stateManager remainingAttachmentCapacity];
}

- (NSUInteger)remainingNumberOfImagesAllowed {
  return [_stateManager remainingNumberOfImagesAllowed];
}

- (ComposeboxAttachmentSelection*)currentAttachmentSelection {
  std::set<web::WebStateID> tabIDs;
  NSMutableArray<ComposeboxPickerImageResult*>* images =
      [[NSMutableArray alloc] init];
  NSMutableArray<NSURL*>* files = [[NSMutableArray alloc] init];
  NSMutableArray<ComposeboxPickerDriveResult*>* driveItems =
      [[NSMutableArray alloc] init];

  for (ComposeboxInputItem* item in _items.containedItems) {
    switch (item.type) {
      case ComposeboxInputItemType::kComposeboxInputItemTypeTab: {
        auto it = _latestTabSelectionMapping.find(item.identifier);
        if (it != _latestTabSelectionMapping.end() && it->second.valid()) {
          tabIDs.insert(it->second);
        }
        break;
      }
      case ComposeboxInputItemType::kComposeboxInputItemTypeImage: {
        if (item.imageProvider) {
          ComposeboxPickerImageResult* result =
              [[ComposeboxPickerImageResult alloc]
                  initWithImageProvider:item.imageProvider
                                assetID:item.assetID
                                 source:item.source];
          [images addObject:result];
        }
        break;
      }
      case ComposeboxInputItemType::kComposeboxInputItemTypePDF:
      case ComposeboxInputItemType::kComposeboxInputItemTypeRawFile: {
        if (item.fileURL) {
          [files addObject:item.fileURL];
        }
        break;
      }
      case ComposeboxInputItemType::kComposeboxInputItemTypeDrive: {
        ComposeboxPickerDriveResult* result =
            [[ComposeboxPickerDriveResult alloc] init];
        result.identifier = item.driveIdentifier;
        result.fileName = item.title;
        result.mimeType = item.driveMimeType;
        [driveItems addObject:result];
        break;
      }
    }
  }

  return [[ComposeboxAttachmentSelection alloc] initWithTabIDs:tabIDs
                                             cachedWebStateIDs:{}
                                                        images:images
                                                         files:files
                                                    driveItems:driveItems];
}

- (void)updateAttachments:(ComposeboxAttachmentSelection*)attachments {
  if (!attachments) {
    return;
  }

  for (ComposeboxPickerImageResult* item in attachments.images) {
    [self processImageItemProvider:item.imageProvider
                           assetID:item.assetID
                            source:item.source
                        completion:nil];
  }

  for (NSURL* url in attachments.files) {
    GURL gurl(url.absoluteString.UTF8String);

    UTType* contentType = nil;
    BOOL accessing = [url startAccessingSecurityScopedResource];
    [url getResourceValue:&contentType forKey:NSURLContentTypeKey error:nil];
    BOOL isPDF = [contentType conformsToType:UTTypePDF];

    auto stopAccessScopedResourcesIfNeeded = ^{
      if (accessing) {
        [url stopAccessingSecurityScopedResource];
      }
    };

    [self processFileURL:gurl
                   isPDF:isPDF
              completion:stopAccessScopedResourcesIfNeeded];
  }
  for (ComposeboxPickerDriveResult* driveItem in attachments.driveItems) {
    [self processDriveFileWithIdentifier:driveItem.identifier
                                    name:driveItem.fileName
                                mimeType:driveItem.mimeType];
  }

  // TODO(crbug.com/512774045): update attachment is called in both embedded an
  // focus flow. Verify metrics recording in both flows.
  [self attachSelectedTabsWithWebStateIDs:attachments.tabIDs
                        cachedWebStateIDs:attachments.cachedWebStateIDs];
}

- (void)applyFocusParams:(ComposeboxFocusParams*)params {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!params) {
    return;
  }

  if ([params hasInitialAttachments]) {
    _awaitingAttachmentSignals = YES;
  }

  _modeHolder.mode = params.toolMode;

  if (params.modelMode != ComposeboxModelOption::kNone) {
    [self setModelOption:params.modelMode explicitUserAction:YES];
  }

  if (params.attachmentList) {
    [self updateAttachments:params.attachmentList];
  }
}

#pragma mark - ComposeboxInputPlateMutator

// Removes an item from the collection.
- (void)removeItem:(ComposeboxInputItem*)item {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  [self.debugLogger
      logEvent:[ComposeboxDebuggerEvent
                   queryAttachmentEvent:composebox_debugger::event::
                                            QueryAttachment::kRemoved
                               withType:[self attachmentEventTypeForItem:item]
                                  title:[self
                                            attachmentEventTitleForItem:item]]];

  [_items removeItem:item];

  if (_contextualSearchSession) {
    _contextualSearchSession->DeleteFile(item.serverToken);
    [self reloadSuggestions];
  }

  [self notifyContextChanged];
}

- (void)sendText:(NSString*)text {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self sendText:text additionalParams:{}];
}

- (void)sendText:(NSString*)text
    additionalParams:(std::map<std::string, std::string>)additionalParams {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Contextual search session can be null when fusebox is disabled.
  if (!_contextualSearchSession) {
    if (_templateURLService) {
      GURL URL = GetDefaultSearchURLForSearchTerms(_templateURLService,
                                                   [text cr_UTF16String]);
      [self didCreateSearchURL:URL];
    }
    return;
  }

  auto advancedToolsParams = [_stateManager additionalQueryParams];
  additionalParams.insert(advancedToolsParams.begin(),
                          advancedToolsParams.end());

  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
      search_url_request_info = std::make_unique<
          ComposeboxQueryController::CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = base::SysNSStringToUTF8(text);
  search_url_request_info->query_start_time = base::Time::Now();
  search_url_request_info->aim_entry_point =
      omnibox::IOS_CHROME_FUSEBOX_ENTRY_POINT;
  search_url_request_info->additional_params = additionalParams;

  __weak __typeof(self) weakSelf = self;
  auto callback =
      base::BindPostTaskToCurrentDefault(base::BindOnce(^(GURL URL) {
        [weakSelf didCreateSearchURL:URL];
      }));

  auto createSearchUrlCallback = base::BindOnce(
      &contextual_search::ContextualSearchSessionHandle::CreateSearchUrl,
      _contextualSearchSession->AsWeakPtr(), std::move(search_url_request_info),
      std::move(callback));

  if (_queryContextualizer) {
    _queryContextualizer->Contextualize(
        /*task_id=*/std::nullopt, base::SysNSStringToUTF8(text),
        /*tabs_to_recontextualize=*/{}, /*tabs_to_force_contextualize=*/{},
        /*on_ineligible_callback=*/base::DoNothing(),
        /*on_processed_callback=*/base::DoNothing(),
        base::BindOnce(
            [](base::OnceClosure closure,
               base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
                   ignored_handle) { std::move(closure).Run(); },
            std::move(createSearchUrlCallback)),
        /*enable_smart_tab_selection=*/false);
  } else {
    std::move(createSearchUrlCallback).Run();
  }
}

- (void)processTab:(web::WebState*)webState
        webStateID:(web::WebStateID)webStateID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(webState);
  std::set<web::WebStateID> tabs = [self allAttachedWebStateIDs];
  tabs.insert(webStateID);
  [self attachSelectedTabsWithWebStateIDs:tabs
                        cachedWebStateIDs:tabs
                     fromExternalWebState:(_webStateList->GetIndexOfWebState(
                                               webState) ==
                                           WebStateList::kInvalidIndex)
                                              ? webState
                                              : nullptr
                                   source:ComposeboxInputItemSource::
                                              kDragAndDrop];
}

- (void)processText:(NSString*)text {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate refineWithText:text];
}

- (void)processFileURL:(GURL)fileURL isPDF:(BOOL)isPDF {
  [self processFileURL:fileURL isPDF:isPDF completion:nil];
}

- (void)processFileURL:(GURL)fileURL
                 isPDF:(BOOL)isPDF
            completion:(void (^)(void))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSString* assetID = base::SysUTF8ToNSString(fileURL.spec());
  if ([_items assetAlreadyLoaded:assetID]) {
    if (completion) {
      completion();
    }
    return;
  }

  // Check file size.
  NSURL* nsURL = net::NSURLWithGURL(fileURL);
  NSError* error = nil;
  NSNumber* fileSize =
      [[nsURL resourceValuesForKeys:@[ NSURLFileSizeKey ]
                              error:&error] objectForKey:NSURLFileSizeKey];
  if (fileSize && [fileSize unsignedLongLongValue] > kMaxFileAttachmentSize) {
    [self.delegate showSnackbarForItemUploadDidFail];
    if (completion) {
      completion();
    }
    return;
  }

  ComposeboxInputItemType itemType =
      isPDF ? ComposeboxInputItemType::kComposeboxInputItemTypePDF
            : ComposeboxInputItemType::kComposeboxInputItemTypeRawFile;

  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:itemType
                              assetID:assetID
                               source:ComposeboxInputItemSource::kFilePicker];
  item.title = base::SysUTF8ToNSString(fileURL.ExtractFileName());
  [self addItem:item];
  item.fileURL = nsURL;
  base::UnguessableToken identifier = item.identifier;

  // Read the data in the background then call `onDataReadForItem`.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadDataFromURL, fileURL),
      base::BindOnce(^(NSData* data) {
        [weakSelf onDataReadForItemWithIdentifier:identifier
                                          fromURL:fileURL
                                         withData:data];
        if (completion) {
          completion();
        }
      }));
}

- (void)processImageItemProvider:(NSItemProvider*)itemProvider
                         assetID:(NSString*)assetID
                          source:(ComposeboxInputItemSource)source {
  [self processImageItemProvider:itemProvider
                         assetID:assetID
                          source:source
                      completion:nil];
}

- (void)processImageItemProvider:(NSItemProvider*)itemProvider
                         assetID:(NSString*)assetID
                          source:(ComposeboxInputItemSource)source
                      completion:(void (^)(void))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  BOOL unableToLoadUIImage =
      ![itemProvider canLoadObjectOfClass:[UIImage class]];

  BOOL assetAlreadyLoaded = [_items assetAlreadyLoaded:assetID];
  if (unableToLoadUIImage || assetAlreadyLoaded) {
    if (completion) {
      completion();
    }
    return;
  }

  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeImage
                              assetID:assetID
                               source:source];
  [self addItem:item];
  item.imageProvider = itemProvider;
  __block base::UnguessableToken identifier = item.identifier;

  __block int requiredNumberOfLoads = 2;
  __weak __typeof(self) weakSelf = self;
  // Load the preview image.
  [itemProvider
      loadPreviewImageWithOptions:nil
                completionHandler:^(UIImage* previewImage, NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadPreviewImage:previewImage
                            forItemWithIdentifier:identifier];
                    requiredNumberOfLoads--;
                    if (requiredNumberOfLoads == 0 && completion) {
                      completion();
                    }
                  });
                }];

  // Concurrently load the full image.
  [itemProvider loadObjectOfClass:[UIImage class]
                completionHandler:^(__kindof id<NSItemProviderReading> object,
                                    NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadFullImage:(UIImage*)object
                         forItemWithIdentifier:identifier];
                    requiredNumberOfLoads--;
                    if (requiredNumberOfLoads == 0 && completion) {
                      completion();
                    }
                  });
                }];
}

- (void)setModelOption:(ComposeboxModelOption)modelOption
    explicitUserAction:(BOOL)explicitUserAction {
  [_stateManager setActiveModel:modelOption
             explicitUserAction:explicitUserAction];
}

- (void)setOmniboxFocused:(bool)focused {
  if (_omniboxFocused == focused) {
    return;
  }
  _omniboxFocused = focused;
  [self requestUIRefresh];
}

#pragma mark - TabPickerSelectionDelegate

- (std::set<web::WebStateID>)allAttachedWebStateIDs {
  std::set<web::WebStateID> webStateIDs;
  for (ComposeboxInputItem* item in _items.containedItems) {
    web::WebStateID webStateID = _latestTabSelectionMapping[item.identifier];

    if (!webStateID.valid()) {
      continue;
    }

    // Include web state IDs of tabs added to composebox context from other
    // web states.
    webStateIDs.insert(webStateID);
  }
  return webStateIDs;
}

- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContext {
  std::set<web::WebStateID> webStateIDs;
  for (ComposeboxInputItem* item in _items.containedItems) {
    web::WebStateID webStateID = _latestTabSelectionMapping[item.identifier];

    if (!webStateID.valid()) {
      continue;
    }

    // Only get web state IDs for tabs in the current web state.
    WebStateSearchCriteria searchCriteria{
        .identifier = webStateID,
        .pinned_state = WebStateSearchCriteria::PinnedState::kAny,
    };

    if (GetWebStateIndex(_webStateList, searchCriteria) !=
        WebStateList::kInvalidIndex) {
      webStateIDs.insert(webStateID);
    }
  }
  return webStateIDs;
}

- (NSUInteger)maxTabAttachmentCount {
  return [_stateManager maxTabAttachmentCount];
}

- (void)attachSelectedTabsWithWebStateIDs:
            (std::set<web::WebStateID>)selectedWebStateIDs
                        cachedWebStateIDs:
                            (std::set<web::WebStateID>)cachedWebStateIDs {
  [self
      attachSelectedTabsWithWebStateIDs:selectedWebStateIDs
                      cachedWebStateIDs:cachedWebStateIDs
                   fromExternalWebState:nullptr
                                 source:ComposeboxInputItemSource::kTabPicker];
}

- (void)removeDeselectedIDs:(std::set<web::WebStateID>)deselectedIDs {
  NSArray<ComposeboxInputItem*>* items = [_items.containedItems copy];
  for (ComposeboxInputItem* item in items) {
    web::WebStateID webStateID = _latestTabSelectionMapping[item.identifier];
    if (webStateID.valid() && deselectedIDs.contains(webStateID)) {
      [self removeItem:item];
      _latestTabSelectionMapping.erase(item.identifier);
    }
  }
}

// Creates an input item for the given `webState` and adds it to the items list.
// Returns the identifier of the created item.
- (base::UnguessableToken)createInputItemForWebState:(web::WebState*)webState
                                              source:(ComposeboxInputItemSource)
                                                         source {
  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeTab
                               source:source];
  item.title = base::SysUTF16ToNSString(webState->GetTitle());
  base::UnguessableToken identifier = item.identifier;
  _latestTabSelectionMapping[identifier] = webState->GetUniqueIdentifier();

  [self addItem:item];

  if (_faviconLoader) {
    __weak __typeof(self) weakSelf = self;

    /// Based on the favicon loader API, this callback could be called twice.
    auto faviconLoadedBlock = ^(FaviconAttributes* attributes, bool cached) {
      if (attributes.faviconImage) {
        [weakSelf didLoadFaviconIcon:attributes.faviconImage
               forItemWithIdentifier:identifier];
      }
    };

    _faviconLoader->FaviconForPageUrl(
        webState->GetVisibleURL(), gfx::kFaviconSize, gfx::kFaviconSize,
        /*fallback_to_google_server=*/true, faviconLoadedBlock);
  }

  return identifier;
}

// Retrieves and attaches web state content (specifically the APC) to an item.
// The content can be fetched from the cache or computed on the fly. An optional
// snapshot of the page can be included.
- (void)attachWebStateContent:(web::WebState*)webState
                   identifier:(base::UnguessableToken)identifier
                 hasCachedAPC:(BOOL)hasCachedAPC {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  if (hasCachedAPC && _persistTabContextAgent) {
    _persistTabContextAgent->GetSingleContextAsync(
        base::NumberToString(weakWebState->GetUniqueIdentifier().identifier()),
        base::BindOnce(^(std::optional<std::unique_ptr<
                             optimization_guide::proto::PageContext>> context) {
          if (context.has_value()) {
            [weakSelf handlePageContextResponse:std::move(context.value())
                                       webState:weakWebState.get()
                                     identifier:identifier];
          } else {
            [weakSelf attachWebState:weakWebState.get()
                          identifier:identifier
                            isCached:NO];
          }
        }));
    return;
  }

  PageContextWrapper* pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:webState
      completionCallback:base::BindOnce(^(
                             PageContextWrapperCallbackResponse response) {
        if (response.has_value()) {
          [weakSelf handlePageContextResponse:std::move(response.value())
                                     webState:weakWebState.get()
                                   identifier:identifier];
        } else {
          [weakSelf handleFailedAttachment:identifier];
        }
      })];

  pageContextWrapper.shouldGetAnnotatedPageContent = YES;
  pageContextWrapper.shouldGetSnapshot = YES;
  // The Page Context execution can take more than the default 1 second on slow
  // devices or bots.
  [pageContextWrapper
      populatePageContextFieldsAsyncWithTimeout:kPageContextTimeout];

  _pageContextWrappers[webState->GetUniqueIdentifier()] = pageContextWrapper;
}

// Transforms the page context into input data and uploads the data after a
// page snapshot is generated.
- (void)handlePageContextResponse:
            (std::unique_ptr<optimization_guide::proto::PageContext>)
                page_context
                         webState:(web::WebState*)webState
                       identifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (!webState || !page_context) {
    return;
  }

  _pageContextWrappers.erase(webState->GetUniqueIdentifier());

  __block std::unique_ptr<lens::ContextualInputData> input_data =
      CreateInputDataFromAnnotatedPageContent(
          base::WrapUnique(page_context->release_annotated_page_content()),
          webState);

  __weak __typeof(self) weakSelf = self;
  SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
      ^(UIImage* image) {
        [weakSelf didRetrieveColorSnapshot:image
                                 inputData:std::move(input_data)
                                identifier:identifier];
      });
}

// Uploads the tab context for the given identifier.
- (void)uploadTabForIdentifier:(base::UnguessableToken)identifier
                     inputData:
                         (std::unique_ptr<lens::ContextualInputData>)inputData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_contextualSearchSession) {
    return;
  }

  web::WebStateID webStateID = _latestTabSelectionMapping[identifier];
  if (!webStateID.valid()) {
    return;
  }

  auto serverToken = _contextualSearchSession->CreateContextToken();

  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  if (item) {
    item.serverToken = serverToken;
  }

  if (_contextualSearchSession) {
    _contextualSearchSession->StartTabContextUploadFlow(
        serverToken, std::move(inputData), GetDefaultImageEncodingOptions());
  }

  [self notifyContextChanged];
}

// Invoked when a file context has been successfully uploaded to the server
// and added to the session.
- (void)onFileContextAdded:(base::UnguessableToken)serverToken
             forIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  if (item) {
    item.serverToken = serverToken;
  }
}

- (void)extractFaviconForCurrentTab {
  if (!_faviconLoader) {
    return;
  }
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }

  __weak __typeof(self) weakSelf = self;

  auto faviconLoadedBlock = ^(FaviconAttributes* attributes, bool cached) {
    if (attributes.faviconImage) {
      [weakSelf setCachedCurrentTabFavicon:attributes.faviconImage];
    }
  };

  _faviconLoader->FaviconForPageUrl(
      webState->GetVisibleURL(), gfx::kFaviconSize, gfx::kFaviconSize,
      /*fallback_to_google_server=*/true, faviconLoadedBlock);
}

- (void)setCachedCurrentTabFavicon:(UIImage*)image {
  _currentTabFavicon = image;
  [self commitUIUpdates];
}

- (void)attachCurrentTabContent {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (![_stateManager canAddMoreAttachments]) {
    [self.delegate showAttachmentLimitError];
    return;
  }
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }

  [self.metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kCurrentTab];

  std::set<web::WebStateID> webStateIDs =
      [self attachedWebStateIDsInCurrentContext];
  webStateIDs.insert(webState->GetUniqueIdentifier());
  [self
      attachSelectedTabsWithWebStateIDs:webStateIDs
                      cachedWebStateIDs:{}
                   fromExternalWebState:nullptr
                                 source:ComposeboxInputItemSource::kCurrentTab];
}

- (void)recordPlusMenuOpenedWithVisibleInternalButtons:
            (const std::vector<FuseboxAttachmentButtonType>&)
                visibleInternalButtons
                                          uiInputState:
                                              (ComposeboxUIInputState*)state {
  [self.metricsRecorder
      recordAttachmentsMenuOpenedWithVisibleButtons:visibleInternalButtons];

  for (const auto& tool : state.allowedTools) {
    [self.metricsRecorder recordToolModeShown:tool];
  }

  for (const auto& model : state.allowedModels) {
    [self.metricsRecorder recordModelModeShown:model];
  }
}

- (void)requestUIRefresh {
  [self commitUIUpdates];
}

#pragma mark - ComposeboxContextUploadObserver

- (void)onContextUploadStatusChanged:(const base::UnguessableToken&)contextToken
                            mimeType:(lens::MimeType)mimeType
                 contextUploadStatus:
                     (contextual_search::ContextUploadStatus)contextUploadStatus
                           errorType:
                               (const std::optional<
                                   contextual_search::ContextUploadErrorType>&)
                                   errorType {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForServerToken:contextToken];

  if (!item) {
    return;
  }

  switch (contextUploadStatus) {
    case contextual_search::ContextUploadStatus::kUploadSuccessful:
      [self setState:ComposeboxInputItemState::kLoaded onItem:item];
      break;
    case contextual_search::ContextUploadStatus::kUploadFailed:
    case contextual_search::ContextUploadStatus::kValidationFailed:
    case contextual_search::ContextUploadStatus::kUploadExpired:
      [self handleFailedAttachment:item.identifier];
      break;
    case contextual_search::ContextUploadStatus::kProcessingSuggestSignalsReady:
      // Signals are ready, we are no longer waiting.
      _awaitingAttachmentSignals = NO;
      [self reloadSuggestions];
      break;
    case contextual_search::ContextUploadStatus::kNotUploaded:
    case contextual_search::ContextUploadStatus::kProcessing:
    case contextual_search::ContextUploadStatus::kUploadStarted:
    case contextual_search::ContextUploadStatus::kUploadReplaced:
      // No-op, as the state is already `Uploading`.
      return;
  }

  [self.consumer updateState:item.state forItemWithIdentifier:item.identifier];
}

#pragma mark - NSNotification

- (void)appDidEnterBackground:(NSNotification*)notification {
  if (_contextualSearchSession) {
    _contextualSearchSession->SetIsBackgrounded(true);
  }
}

- (void)appWillEnterForeground:(NSNotification*)notification {
  if (_contextualSearchSession) {
    _contextualSearchSession->SetIsBackgrounded(false);
  }
}

#pragma mark - Private

// Whether the current instance is associated with cobrowse.
- (BOOL)isCobrowse {
  return _entrypoint == ComposeboxEntrypoint::kCobrowse;
}

// Sends a Cobrowse text followup.
- (void)sendAIMFollowup:(NSString*)text {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_contextualSearchSession) {
    return;
  }

  std::optional<contextual_search::InputState> inputState =
      _stateManager.inputState;
  if (!inputState.has_value()) {
    return;
  }

  std::unique_ptr<contextual_search::ContextualSearchContextController::
                      CreateClientToAimRequestInfo>
      request_info = std::make_unique<
          contextual_search::ContextualSearchContextController::
              CreateClientToAimRequestInfo>();
  request_info->query_text = base::SysNSStringToUTF8(text);
  request_info->query_start_time = base::Time::Now();
  request_info->file_tokens =
      _contextualSearchSession->GetSubmittedContextTokens();

  request_info->active_tool = inputState->active_tool;
  request_info->active_model = inputState->active_model;

  lens::ClientToAimMessage message =
      _contextualSearchSession->CreateClientToAimRequest(
          std::move(request_info));

  [self.URLLoader prepareLoadWithClientToAimMessage:message];
}

// Informs the model of a context change (e.g.; attachment added or deleted).
- (void)notifyContextChanged {
  [_stateManager onContextChanged];
}

// Updates the awaiting attachment signals state. Note that this flag is only
// set to YES during the initial focus flow (triggering the composebox with
// initial attachments). We stop awaiting signals when there are no more items
// in the loading or uploading state.
- (void)updateAwaitingAttachmentSignalsState {
  if (!_awaitingAttachmentSignals) {
    return;
  }

  // Check if there are any items still actively loading or uploading.
  BOOL hasPendingItems = NO;
  for (ComposeboxInputItem* item in _items.containedItems) {
    BOOL isLoadingOrUploading =
        item.state == ComposeboxInputItemState::kLoading ||
        item.state == ComposeboxInputItemState::kUploading;
    if (isLoadingOrUploading) {
      hasPendingItems = YES;
      break;
    }
  }

  // If no items are pending, we are no longer awaiting signals.
  if (!hasPendingItems) {
    _awaitingAttachmentSignals = NO;
  }
}

// Adds an item to the collection.
- (void)addItem:(ComposeboxInputItem*)item {
  [self.debugLogger
      logEvent:[ComposeboxDebuggerEvent
                   queryAttachmentEvent:composebox_debugger::event::
                                            QueryAttachment::kAdded
                               withType:[self attachmentEventTypeForItem:item]
                                  title:[self
                                            attachmentEventTitleForItem:item]]];
  [_items addItem:item];
}

// Sets the state for a given item.
- (void)setState:(ComposeboxInputItemState)state
          onItem:(ComposeboxInputItem*)item {
  item.state = state;

  composebox_debugger::event::QueryAttachment eventType;
  switch (state) {
    case ComposeboxInputItemState::kUploading:
      eventType = composebox_debugger::event::QueryAttachment::kAdded;
      break;
    case ComposeboxInputItemState::kLoaded:
      eventType = composebox_debugger::event::QueryAttachment::
          kUploadCompletedSuccessfully;
      break;
    case ComposeboxInputItemState::kError:
      eventType = composebox_debugger::event::QueryAttachment::kUploadFailed;
      break;
    default:
      return;
  }

  [self.debugLogger
      logEvent:[ComposeboxDebuggerEvent
                   queryAttachmentEvent:eventType
                               withType:[self attachmentEventTypeForItem:item]
                                  title:[self
                                            attachmentEventTitleForItem:item]]];
  [self updateAwaitingAttachmentSignalsState];
}

// Returns the attachment evewnt title for a given item.
- (NSString*)attachmentEventTitleForItem:(ComposeboxInputItem*)item {
  return base::SysUTF8ToNSString(item.identifier.ToString());
}

// Returns the attachment type for a given item.
- (composebox_debugger::AttachmentType)attachmentEventTypeForItem:
    (ComposeboxInputItem*)item {
  switch (item.type) {
    case ComposeboxInputItemType::kComposeboxInputItemTypeImage:
      return composebox_debugger::AttachmentType::kImage;
    case ComposeboxInputItemType::kComposeboxInputItemTypeRawFile:
    case ComposeboxInputItemType::kComposeboxInputItemTypePDF:
    case ComposeboxInputItemType::kComposeboxInputItemTypeDrive:
      return composebox_debugger::AttachmentType::kFile;
    case ComposeboxInputItemType::kComposeboxInputItemTypeTab:
      return composebox_debugger::AttachmentType::kTab;
  }
}

// Helper for `-attachSelectedTabsWithWebStateIDs:cachedWebStateIDs:`. Attaches
// the selected tabs. `cachedWebStateIDs` contains the IDs of the tabs that have
// their content cached. When a tab attachment is initiated for tabs with a
// different web state than the current window (such as a UI drag-and-drop
// action across windows), an `externalWebState` is provided to make tabs from
// another eligible browser window visible. Otherwise, `externalWebState` should
// be `nullptr`.
- (void)attachSelectedTabsWithWebStateIDs:
            (std::set<web::WebStateID>)selectedWebStateIDs
                        cachedWebStateIDs:
                            (std::set<web::WebStateID>)cachedWebStateIDs
                     fromExternalWebState:(web::WebState*)externalWebState
                                   source:(ComposeboxInputItemSource)source {
  _pageContextWrappers.clear();

  // Remove tabs from context that were deselected in the tab picker.
  std::set<web::WebStateID> alreadyProcessedIDsFromCurrentWebState =
      [self attachedWebStateIDsInCurrentContext];
  composebox::TabDiff currentContextDiff = composebox::ComputeTabDiff(
      alreadyProcessedIDsFromCurrentWebState, selectedWebStateIDs);
  [self removeDeselectedIDs:currentContextDiff.removed];

  // Prevent duplicate tabs from external web states from being added to
  // context.
  std::set<web::WebStateID> alreadyProcessedIDs = [self allAttachedWebStateIDs];
  composebox::TabDiff allDiff =
      composebox::ComputeTabDiff(alreadyProcessedIDs, selectedWebStateIDs);

  if (allDiff.added.empty()) {
    return;
  }

  for (const web::WebStateID& candidateID : allDiff.added) {
    web::WebState* candidateWebState;

    if (!externalWebState) {
      WebStateSearchCriteria tabSearchCriteria = WebStateSearchCriteria{
          .identifier = candidateID,
          .pinned_state = WebStateSearchCriteria::PinnedState::kAny,
      };
      int tabIndex = GetWebStateIndex(_webStateList.get(), tabSearchCriteria);
      candidateWebState = _webStateList->GetWebStateAt(tabIndex);
    } else {
      candidateWebState = externalWebState;
    }

    base::UnguessableToken identifier =
        [self createInputItemForWebState:candidateWebState source:source];
    [self attachWebState:candidateWebState
              identifier:identifier
                isCached:cachedWebStateIDs.contains(candidateID)];
  }
}

// Helper for
// `-attachSelectedTabsWithWebStateIDs:cachedWebStateIDs:fromExternalWebState:`.
// Attaches tabs to composebox context.
- (void)attachWebState:(web::WebState*)webState
            identifier:(base::UnguessableToken)identifier
              isCached:(BOOL)isCached {
  // When attaching a tab, we must also send its snapshot. The
  // snapshotTabHelper is only created if the webstate is realized. If the
  // tab's APC is not cached, we must also load the webstate so its APC can be
  // extracted on the fly.
  __weak __typeof(self) weakSelf = self;

  if (isCached) {
    [_webStateDeferredExecutor webState:webState
                    executeOnceRealized:^{
                      [weakSelf attachWebStateContent:webState
                                           identifier:identifier
                                         hasCachedAPC:YES];
                    }];
  } else {
    [_webStateDeferredExecutor webState:webState
                      executeOnceLoaded:^(BOOL success) {
                        if (!success) {
                          [weakSelf handleFailedAttachment:identifier];
                          return;
                        }
                        [weakSelf attachWebStateContent:webState
                                             identifier:identifier
                                           hasCachedAPC:NO];
                      }];
  }
}

// Returns YES if the currently active web state is attached to the composebox.
- (BOOL)isActiveTabAttached {
  if (!_webStateList) {
    return NO;
  }

  web::WebState* webState = _webStateList->GetActiveWebState();

  if (!webState) {
    return NO;
  }
  return [self attachedWebStateIDsInCurrentContext].contains(
      webState->GetUniqueIdentifier());
}

// Loads the aim query once the URL is generated by the Contextual search
// session.
- (void)didCreateSearchURL:(GURL)URL {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self recordNavigationInitiated];

  // TODO(crbug.com/40280872): Handle AIM enabled in the query controller.
  if ([_modeHolder isRegularSearch]) {
    URL = net::AppendOrReplaceQueryParameter(URL, "udm", "24");
  } else if (_modeHolder.mode == ComposeboxMode::kAIM) {
    // If the active tab is attached to the composebox, an AIM query should
    // invoke the Assistant directly using the query URL instead of routing
    // through the standard search navigation flow.
    if (IsAimCobrowseEnabled() && [self isActiveTabAttached]) {
      CobrowseContext* context = [[CobrowseContext alloc] initWithURL:URL];
      context.attachedItems = _items.containedItems;
      if (_cobrowseBrowserAgent) {
        _cobrowseBrowserAgent->SetCobrowseContext(context);
        _cobrowseBrowserAgent->SetSessionActive(true);
      }
      [_browserCoordinatorHandler hideComposebox];
      [_sceneHandler showAssistant];
      return;
    }
  }

  UrlLoadParams params = CreateOmniboxUrlLoadParams(
      URL, /*post_content=*/nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_GENERATED,
      /*destination_url_entered_without_scheme=*/false, _isIncognito);

  [self.URLLoader loadURLParams:params];
}

// Returns the current AutocompleteRequestType based on _modeHolder.mode
- (AutocompleteRequestType)currentAutocompleteRequestType {
  switch (_modeHolder.mode) {
    case ComposeboxMode::kRegularSearch:
      return AutocompleteRequestType::kSearch;
    case ComposeboxMode::kAIM:
      return AutocompleteRequestType::kAIMode;
    case ComposeboxMode::kImageGeneration:
      return AutocompleteRequestType::kImageGeneration;
    case ComposeboxMode::kCanvas:
      return AutocompleteRequestType::kCanvas;
    case ComposeboxMode::kDeepSearch:
      return AutocompleteRequestType::kDeepSearch;
  }
}

- (BOOL)awaitingAttachmentSignals {
  return _awaitingAttachmentSignals;
}

// Centralized entrypoint to record navigation metrics exactly once.
- (void)recordNavigationInitiated {
  if (_inNavigation) {
    return;
  }
  _inNavigation = YES;

  [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                            [self currentAutocompleteRequestType]];
  [self.metricsRecorder
      recordAttachCountAtSubmission:_items.tabsCount
                            forType:ComposeboxMetricsAttachmentType::kTab];
  [self.metricsRecorder
      recordAttachCountAtSubmission:_items.imagesCount
                            forType:ComposeboxMetricsAttachmentType::kImage];
  // Raw file is used as the metric type is the same for raw files and PDFs.
  [self.metricsRecorder
      recordAttachCountAtSubmission:_items.filesCount
                            forType:ComposeboxMetricsAttachmentType::kRawFile];
  [_stateManager recordInputStateOnSubmission];
}

// Records whether the session resulted in navigation.
- (void)recordSessionEndMetrics {
  [self.metricsRecorder
      recordComposeboxFocusResultedInNavigation:_inNavigation
                                withAttachments:!_items.empty
                                    requestType:
                                        [self currentAutocompleteRequestType]];

  std::vector<lens::MimeType> types = MimeTypesFromCollection(_items);
  [_stateManager recordInputStateOnSessionEndWithNavigation:_inNavigation
                                                  mimeTypes:types];
}

// Reloads the displayed suggestions based on the attachments/modeHolder.
- (void)reloadSuggestions {
  [self updateAwaitingAttachmentSignalsState];

  BOOL shouldRestartAutocomplete = _items.count <= 1;

  if (_items.count > 1) {
    shouldRestartAutocomplete =
        IsComposeboxFetchContextualSuggestionsForMultiAttachmentsEnabled();
  }
  [self.delegate
      reloadAutocompleteSuggestionsRestarting:shouldRestartAutocomplete];
}

// Handles the loaded preview `image` for the item with the given `identifier`.
- (void)didLoadPreviewImage:(UIImage*)previewImage
      forItemWithIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  if (!item) {
    return;
  }
  // Only set the preview if a preview doesn't already exist. This prevents
  // overwriting the full-res image if it arrives first.
  if (previewImage && !item.previewImage) {
    item.previewImage = previewImage;
    [self.consumer updateState:item.state
         forItemWithIdentifier:item.identifier];
  }
}

// Handles the loaded favicon `image` for the item with the given `identifier`.
- (void)didLoadFaviconIcon:(UIImage*)faviconImage
     forItemWithIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  if (!item) {
    return;
  }

  // Update the item's leading icon with the latest fetched favicon.
  if (faviconImage && faviconImage != item.leadingIconImage) {
    item.leadingIconImage = faviconImage;
    [self.consumer updateState:item.state
         forItemWithIdentifier:item.identifier];
  }
}

// Handles the loaded full `image` for the item with the given `identifier`.
- (void)didLoadFullImage:(UIImage*)image
    forItemWithIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  if (!item) {
    return;
  }

  if (!image) {
    [self setState:ComposeboxInputItemState::kError onItem:item];
    [self.consumer updateState:item.state
         forItemWithIdentifier:item.identifier];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf didFinishSimulatedLoadForImage:image
                                  itemIdentifier:identifier];
      }),
      GetImageLoadDelay());
}

// Called after the simulated image load delay for the item with the given
// `identifier`. This simulates a network delay for development purposes.
- (void)didFinishSimulatedLoadForImage:(UIImage*)image
                        itemIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  if (!item) {
    return;
  }

  [self setState:ComposeboxInputItemState::kUploading onItem:item];
  [self.consumer updateState:item.state forItemWithIdentifier:item.identifier];

  if (!item.previewImage) {
    item.previewImage =
        ResizeImage(image, composeboxAttachments::kImageInputItemSize,
                    ProjectionMode::kAspectFill);
    [self updateConsumerItems];
    [self commitUIUpdates];
  }

  base::OnceClosure task;
  __weak __typeof(self) weakSelf = self;
  if (ShouldForceUploadFailure()) {
    // Determine a server token to use for the simulated failure.
    // Using the identifier as the server token for simulation.
    item.serverToken = identifier;
    task = base::BindOnce(^{
      [weakSelf
          onContextUploadStatusChanged:identifier
                              mimeType:lens::MimeType::kImage
                   contextUploadStatus:contextual_search::ContextUploadStatus::
                                           kUploadFailed
                             errorType:std::nullopt];
    });
  } else {
    task = base::BindOnce(^{
      [weakSelf uploadImage:image itemIdentifier:identifier];
    });
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(task), GetUploadDelay());
}

// Handles the image data after it has been converted, and uploads it.
- (void)handleImageUploadWithData:(NSData*)data
                forItemIdentifier:(base::UnguessableToken)identifier
                          options:(const lens::ImageEncodingOptions&)options {
  if (!data) {
    [self handleFailedAttachment:identifier];
    return;
  }

  if (!_contextualSearchSession) {
    return;
  }

  mojo_base::BigBuffer buffer(base::apple::NSDataToSpan(data));

  auto serverToken = _contextualSearchSession->CreateContextToken();
  // Register the file context with the UI as soon as the token is created so
  // that it can listen to all file upload events.
  [self onFileContextAdded:serverToken forIdentifier:identifier];
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  std::string fileName = item ? base::SysNSStringToUTF8(item.title) : "";
  _contextualSearchSession->StartFileContextUploadFlow(
      serverToken, fileName, kPortableNetworkGraphicMimeType, std::move(buffer),
      options);
  [self notifyContextChanged];
}

// Uploads the `image` for the item with the given `identifier`.
- (void)uploadImage:(UIImage*)image
     itemIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  if (!item || !_contextualSearchSession) {
    return;
  }

  // UIImagePNGRepresentation is an expensive operation. We execute this on a
  // background thread to prevent blocking the UI, especially during batch
  // processing.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&UIImagePNGRepresentation, image),
      base::BindOnce(^(NSData* data) {
        [weakSelf handleImageUploadWithData:data
                          forItemIdentifier:identifier
                                    options:GetDefaultImageEncodingOptions()];
      }));
}

// Handles uploading the context after the snapshot is generated.
- (void)didRetrieveColorSnapshot:(UIImage*)image
                       inputData:
                           (std::unique_ptr<lens::ContextualInputData>)inputData
                      identifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self didLoadPreviewImage:image forItemWithIdentifier:identifier];

  if (!image) {
    [self uploadTabForIdentifier:identifier inputData:std::move(inputData)];
    return;
  }

  __block std::unique_ptr<lens::ContextualInputData> blockInputData =
      std::move(inputData);
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&UIImagePNGRepresentation, image),
      base::BindOnce(^(NSData* data) {
        [weakSelf handleTabUploadWithPNGData:data
                                   inputData:std::move(blockInputData)
                                  identifier:identifier];
      }));
}

// Handles the tab data after it has been converted, and uploads it.
- (void)handleTabUploadWithPNGData:(NSData*)data
                         inputData:(std::unique_ptr<lens::ContextualInputData>)
                                       inputData
                        identifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (data) {
    std::vector<uint8_t> image_vector_data([data length]);
    [data getBytes:image_vector_data.data() length:[data length]];
    inputData->viewport_screenshot_bytes = std::move(image_vector_data);
  }
  [self uploadTabForIdentifier:identifier inputData:std::move(inputData)];
}

// Handles the read `data` from the given `url` for the item with the given
// `identifier`. This is the callback for the asynchronous file read.
- (void)onDataReadForItemWithIdentifier:(base::UnguessableToken)identifier
                                fromURL:(GURL)url
                               withData:(NSData*)data {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  if (!item) {
    return;
  }

  if (!data) {
    [self setState:ComposeboxInputItemState::kError onItem:item];
    [self.consumer updateState:item.state
         forItemWithIdentifier:item.identifier];
    return;
  }

  // Start the file upload immediately.
  [self setState:ComposeboxInputItemState::kUploading onItem:item];
  [self.consumer updateState:item.state forItemWithIdentifier:item.identifier];

  if (_contextualSearchSession) {
    mojo_base::BigBuffer buffer(base::apple::NSDataToSpan(data));
    auto serverToken = _contextualSearchSession->CreateContextToken();
    // Register the file context with the UI as soon as the token is created so
    // that it can listen to all file upload events.
    [self onFileContextAdded:serverToken forIdentifier:identifier];
    std::string fileName = base::SysNSStringToUTF8(item.title);
    std::string mimeType;
    if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeRawFile) {
      mimeType = "application/octet-stream";
      if (item.fileURL) {
        UTType* contentType = nil;
        [item.fileURL getResourceValue:&contentType
                                forKey:NSURLContentTypeKey
                                 error:nil];
        if (contentType.preferredMIMEType) {
          mimeType = base::SysNSStringToUTF8(contentType.preferredMIMEType);
        }
      }
    } else {
      mimeType = kAdobePortableDocumentFormatMimeType;
    }
    _contextualSearchSession->StartFileContextUploadFlow(
        serverToken, fileName, mimeType, std::move(buffer),
        GetDefaultImageEncodingOptions());
    [self notifyContextChanged];
  }

  // Concurrently, generate a preview for the UI.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GeneratePDFPreview, data),
      base::BindOnce(^(UIImage* preview) {
        [weakSelf didLoadPreviewImage:preview forItemWithIdentifier:identifier];
      }));
}

- (void)processDriveFileWithIdentifier:(NSString*)identifier
                                  name:(NSString*)name
                              mimeType:(NSString*)mimeType {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ([_items assetAlreadyLoaded:identifier]) {
    return;
  }

  if (!_contextualSearchSession) {
    return;
  }

  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeDrive
                              assetID:identifier
                               source:ComposeboxInputItemSource::kDrivePicker];
  item.title = name;
  item.driveIdentifier = identifier;
  item.driveMimeType = mimeType;

  [self addItem:item];

  [self setState:ComposeboxInputItemState::kUploading onItem:item];
  [self.consumer updateState:item.state forItemWithIdentifier:item.identifier];

  auto serverToken = _contextualSearchSession->CreateContextToken();
  [self onFileContextAdded:serverToken forIdentifier:item.identifier];

  contextual_search::ContextualSearchSessionHandle::DriveUploadParams params;
  params.drive_id = base::SysNSStringToUTF8(identifier);
  params.mime_type = base::SysNSStringToUTF8(mimeType);
  params.file_name = base::SysNSStringToUTF8(name);

  _contextualSearchSession->StartDriveContextUploadFlow(serverToken, params);
  [self notifyContextChanged];
}

- (BOOL)compactModeRequired {
  BOOL dseGoogle = [_stateManager isDSEGoogle];
  BOOL eligibleToAIM = [_stateManager isEligibleToAIM];
  BOOL allowsMultimodalActions = dseGoogle && eligibleToAIM;

  // If multimodal actions are disabled (e.g., when DSE is not Google), compact
  // mode is used to display the simpler input method, regardless the treatment.
  if (!allowsMultimodalActions) {
    return !_isMultiline;
  }

  if (!IsComposeboxCompactModeEnabled()) {
    return NO;
  }

  BOOL forceExpansionOnFocus = self.isCobrowse && _omniboxFocused;
  if (forceExpansionOnFocus) {
    return NO;
  }

  std::set<ComposeboxMode> modesAllowingCompact;
  if (self.isCobrowse) {
    modesAllowingCompact = {ComposeboxMode::kAIM,
                            ComposeboxMode::kRegularSearch};
  } else {
    modesAllowingCompact = {ComposeboxMode::kRegularSearch};
  }

  BOOL alwaysExpandedForCurrentMode =
      !modesAllowingCompact.contains(_modeHolder.mode);

  BOOL requiresExpansion = _isMultiline || alwaysExpandedForCurrentMode;
  return !requiresExpansion;
}

#pragma mark - ComposeboxOmniboxClientDelegate

- (web::WebState*)webState {
  return _webStateList->GetActiveWebState();
}

- (std::optional<contextual_search::InputState>)inputState {
  return _stateManager.inputState;
}

- (std::optional<lens::proto::LensOverlaySuggestInputs>)suggestInputs {
  if (!_contextualSearchSession) {
    return std::nullopt;
  }
  return _contextualSearchSession->GetSuggestInputs();
}

- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
               URLLoadParams:(const UrlLoadParams&)URLLoadParams
                isSearchType:(BOOL)isSearchType {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (![self isActiveTabAttached] &&
      _entrypoint != ComposeboxEntrypoint::kCobrowse) {
    // If we are initiating a new navigation while the active tab is not
    // attached to the composebox, hide the tab-specific assistant sheet.
    [_sceneHandler hideAssistant];
  }

  BOOL isAimFollowup = IsAimCobrowseEnabled() &&
                       (_entrypoint == ComposeboxEntrypoint::kCobrowse);

  if (isAimFollowup) {
    [self sendAIMFollowup:[NSString cr_fromString16:text]];
    return;
  }

  switch (_modeHolder.mode) {
    case ComposeboxMode::kRegularSearch:
      [self recordNavigationInitiated];
      [self.URLLoader loadURLParams:URLLoadParams];
      break;
    case ComposeboxMode::kAIM:
      if (IsAimURL(destinationURL)) {
        [self sendText:[NSString cr_fromString16:text]
            additionalParams:lens::GetParametersMapWithoutQuery(
                                 destinationURL)];
      } else {
        [self sendText:[NSString cr_fromString16:text]];
      }
      break;
    case ComposeboxMode::kImageGeneration:
    case ComposeboxMode::kCanvas:
    case ComposeboxMode::kDeepSearch:
      [self sendText:[NSString cr_fromString16:text]];
      break;
  }
}

- (void)omniboxDidChangeText:(const std::u16string&)text
               isSearchQuery:(BOOL)isSearchQuery
         userInputInProgress:(BOOL)userInputInProgress {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  BOOL hasText = text.length() > 0;
  if (hasText == _hasText && _userInputInProgress == userInputInProgress &&
      _isSearchQuery == isSearchQuery) {
    return;
  }
  _hasText = hasText;
  _userInputInProgress = userInputInProgress;
  _isSearchQuery = isSearchQuery;

  [self commitUIUpdates];
}

- (ComposeboxMode)composeboxMode {
  return _modeHolder.mode;
}

#pragma mark - Private helpers

// Whether the user can ask about the current Tab.
- (BOOL)canAskAboutCurrentTab {
  return _entrypoint != ComposeboxEntrypoint::kCobrowse &&
         IsAskAboutThisPageEnabled() && [self canAttachActiveTab];
}

// Whether the current tab is attachable.
- (BOOL)canAttachActiveTab {
  return [_stateManager canAttachActiveTabWithAttachedWebStateIDs:
                            [self attachedWebStateIDsInCurrentContext]];
}

- (void)handleFailedAttachment:(base::UnguessableToken)identifier {
  [self.delegate showSnackbarForItemUploadDidFail];
  ComposeboxInputItem* item = [_items itemForIdentifier:identifier];
  [self.debugLogger
      logEvent:[ComposeboxDebuggerEvent
                   queryAttachmentEvent:composebox_debugger::event::
                                            QueryAttachment::kUploadFailed
                               withType:[self attachmentEventTypeForItem:item]
                                  title:[self
                                            attachmentEventTitleForItem:item]]];

  [self removeItem:item];
}

/// Updates the consumer items.
- (void)updateConsumerItems {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.consumer setItems:_items.containedItems];
}

- (void)updateButtonsVisibility {
  using enum ComposeboxInputPlateControls;
  BOOL compactMode = [self compactModeRequired];
  BOOL hasAttachments = !_items.empty;
  BOOL hasContent = hasAttachments || _hasText;
  BOOL dseGoogle = [_stateManager isDSEGoogle];
  BOOL eligibleToAIM = [_stateManager isEligibleToAIM];
  BOOL lensAvailable = lens_availability::CheckAvailabilityForLensEntryPoint(
      LensEntrypoint::Composebox, [_stateManager isDSEGoogle]);
  BOOL compactInCobrowse = compactMode && self.isCobrowse;
  BOOL allowsMultimodalActions =
      dseGoogle && eligibleToAIM && !compactInCobrowse;
  BOOL canSend = hasContent && !compactMode && allowsMultimodalActions;
  BOOL showShortcuts =
      !hasContent && !canSend &&
      !base::FeatureList::IsEnabled(kHideFuseboxVoiceLensActions);
  // Hide the plus button is different from !allowsMultimodalActions. When the
  // plus button is hidden, the user can still use multimodal actions from other
  // sources such as drag and drop.
  BOOL hidePlusButton = NO;
  if (IsComposeboxConditionalPlusButtonEnabled() && !self.isCobrowse &&
      _modeHolder.isRegularSearch && compactMode) {
    BOOL isPreEditURL = !_userInputInProgress && _hasText;
    BOOL isURLQuery = _userInputInProgress && _hasText && !_isSearchQuery;
    hidePlusButton = isURLQuery;
    if (GetComposeboxConditionalPlusButtonVariant() ==
            ComposeboxConditionalPlusButtonVariant::kHideInPreEdit &&
        isPreEditURL) {
      hidePlusButton = YES;
    }
  }

  BOOL showLeadingImage =
      !self.isCobrowse &&
      (!compactMode || !allowsMultimodalActions || hidePlusButton);
  BOOL allowsAIMControl = _entrypoint != ComposeboxEntrypoint::kCobrowse;
  BOOL shouldPersistAIMButton = allowsAIMControl &&
                                IsComposeboxAIMNudgeEnabled() && !compactMode &&
                                allowsMultimodalActions;

  ComposeboxInputPlateControls leadingAction =
      (allowsMultimodalActions && !hidePlusButton) ? kPlus : kNone;

  ComposeboxInputPlateControls leadingImage =
      showLeadingImage ? kLeadingImage : kNone;

  ComposeboxInputPlateControls modeSwitchButton;
  switch (_modeHolder.mode) {
    case ComposeboxMode::kAIM:
      modeSwitchButton = allowsAIMControl ? kAIM : kNone;
      break;
    case ComposeboxMode::kImageGeneration:
      modeSwitchButton = kCreateImage;
      break;
    case ComposeboxMode::kRegularSearch:
      modeSwitchButton = shouldPersistAIMButton ? kAIM : kNone;
      break;
    case ComposeboxMode::kCanvas:
      modeSwitchButton = kCanvas;
      break;
    case ComposeboxMode::kDeepSearch:
      modeSwitchButton = kDeepSearch;
      break;
  }

  ComposeboxInputPlateControls askAboutThisPage;

  if (!compactMode && [self canAskAboutCurrentTab] && allowsMultimodalActions &&
      !modeSwitchButton) {
    askAboutThisPage = kAskAboutThisPage;
  } else {
    askAboutThisPage = kNone;
  }

  ComposeboxInputPlateControls trailingAction = kNone;
  if (canSend) {
    trailingAction = kSend;
  } else if (showShortcuts) {
    trailingAction |= kVoice;
    trailingAction |= lensAvailable ? kLens : kQRScanner;
  }

  ComposeboxInputPlateControls visibleControls =
      (leadingImage | leadingAction | modeSwitchButton | askAboutThisPage |
       trailingAction);

  [self.consumer updateVisibleControls:visibleControls];
}

/// Updates the consumer whether to show in compact mode.
- (void)updateCompactMode {
  BOOL compact = [self compactModeRequired];
  if (compact != _compact) {
    [self.debugLogger
        logEvent:[ComposeboxDebuggerEvent
                     composeboxGeneralEvent:
                         compact ? composebox_debugger::event::Composebox::
                                       kCompactModeEnabled
                                 : composebox_debugger::event::Composebox::
                                       kCompactModeDisabled]];
  }
  [self.consumer setCompact:compact];
  _compact = compact;
}

/// Updates the consumer UI input state.
- (void)updateUIInputState {
  ComposeboxUIInputState* state = [_stateManager
      computeUIInputStateWithFavicon:_currentTabFavicon
                 attachedWebStateIDs:[self
                                         attachedWebStateIDsInCurrentContext]];
  [self.consumer setUIInputState:state];
}

// Pushes the batched UI updates to the consumer.
- (void)commitUIUpdates {
  _isUpdatingCompactMode = YES;

  [self updateButtonsVisibility];
  [self updateCompactMode];
  [self updateUIInputState];

  _isUpdatingCompactMode = NO;
}

#pragma mark - ComposeboxInputStateManagerDelegate

- (void)inputStateManager:(ComposeboxInputStateManager*)manager
             didChangeMode:(ComposeboxMode)mode
    invalidatedAttachments:(NSArray<ComposeboxInputItem*>*)invalidatedItems {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  for (ComposeboxInputItem* item in invalidatedItems) {
    if (_contextualSearchSession) {
      _contextualSearchSession->DeleteFile(item.serverToken);
    }
  }
  [_items removeItems:invalidatedItems];

  if (invalidatedItems.count > 0) {
    [self notifyContextChanged];
    [self updateAwaitingAttachmentSignalsState];
  }

  BOOL transitionedToAIMode = mode != ComposeboxMode::kRegularSearch &&
                              _previousMode == ComposeboxMode::kRegularSearch;

  if (transitionedToAIMode) {
    [self.metricsRecorder
        recordTextEditedBeforeAiMode:(_userInputInProgress && _hasText)];
  }
  _previousMode = mode;
  [self reloadSuggestions];
}

- (void)inputStateManagerDidUpdateUIState:
    (ComposeboxInputStateManager*)manager {
  [self commitUIUpdates];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
      LensEntrypoint::Composebox, [_stateManager isDSEGoogle]);
  [self commitUIUpdates];
}

- (void)templateURLServiceShuttingDown:(TemplateURLService*)urlService {
  CHECK_EQ(urlService, _templateURLService);
  _templateURLService = nullptr;
}

#pragma mark - TextFieldViewContainingHeightDelegate

- (void)textFieldViewContaining:(UIView<TextFieldViewContaining>*)sender
                didChangeHeight:(CGFloat)height {
  // Prevent re-entrant calls if `_isMultiline` changes when updating compact
  // mode.
  if (_isUpdatingCompactMode) {
    return;
  }
  _isMultiline = sender.numberOfLines > 1;
  [self commitUIUpdates];
  [self.consumer updatePreferredContentSizeForNewTextFieldHeight];
}

#pragma mark - ComposeboxInputItemCollectionDelegate

- (void)composeboxInputItemCollectionDidUpdateItems:
    (ComposeboxInputItemCollection*)composeboxInputItemCollection {
  [self updateConsumerItems];
  [self commitUIUpdates];
  [_stateManager onItemsUpdated];
}

#pragma mark - VoiceSearchDelegate

- (void)voiceSearchDidReceiveSearchQuery:(NSString*)query {
  [self sendText:query];
}

#pragma mark - ComposeboxQueryContextualizerDelegate

- (GURL)tabURLForTabID:(contextual_tasks::QueryContextualizer::TabId)tabID {
  // No-op. Recontextualization is only needed for the contextual-tasks
  // composebox handler.
  return GURL();
}

- (SessionID)tabSessionIDForTabID:
    (contextual_tasks::QueryContextualizer::TabId)tabID {
  // No-op. Recontextualization is only needed for the contextual-tasks
  // composebox handler.
  return SessionID::InvalidValue();
}

- (void)
    getPageContextForTabID:(contextual_tasks::QueryContextualizer::TabId)tabID
                  callback:(base::OnceCallback<void(
                                std::unique_ptr<lens::ContextualInputData>)>)
                               callback {
  // No-op. Recontextualization is only needed for the contextual-tasks
  // composebox handler.
  if (callback) {
    std::move(callback).Run(nullptr);
  }
}

- (bool)isTabValid:(contextual_tasks::QueryContextualizer::TabId)tabID {
  // No-op. Recontextualization is only needed for the contextual-tasks
  // composebox handler.
  return false;
}

- (std::optional<lens::ImageEncodingOptions>)tabViewportEncodingOptions {
  // No-op. Recontextualization is only needed for the contextual-tasks
  // composebox handler.
  return std::nullopt;
}

- (contextual_search::ContextualSearchSessionHandle*)getOrCreateSessionHandle {
  return _contextualSearchSession.get();
}

#pragma mark - WebStateDeferredExecutorDelegate

- (void)webStateDeferredExecutor:(WebStateDeferredExecutor*)executor
                willLoadWebState:(web::WebState*)webState {
  ComposeboxDebuggerEvent* event = [ComposeboxDebuggerEvent
       tabEvent:composebox_debugger::event::Tabs::kWillLoadTab
      withTitle:base::SysUTF16ToNSString(webState->GetTitle())
          tabID:webState->GetUniqueIdentifier().identifier()];
  [self.debugLogger logEvent:event];
}

- (void)webStateDeferredExecutor:(WebStateDeferredExecutor*)executor
                 didLoadWebState:(web::WebState*)webState
                         success:(BOOL)success {
  composebox_debugger::event::Tabs tabEvent =
      success ? composebox_debugger::event::Tabs::kDidLoadTab
              : composebox_debugger::event::Tabs::kFailedToLoadTab;
  ComposeboxDebuggerEvent* event = [ComposeboxDebuggerEvent
       tabEvent:tabEvent
      withTitle:base::SysUTF16ToNSString(webState->GetTitle())
          tabID:webState->GetUniqueIdentifier().identifier()];
  [self.debugLogger logEvent:event];
}

- (void)webStateDeferredExecutor:(WebStateDeferredExecutor*)executor
        willForceRealizeWebState:(web::WebState*)webState {
  ComposeboxDebuggerEvent* event = [ComposeboxDebuggerEvent
       tabEvent:composebox_debugger::event::Tabs::kWillRealizeTab
      withTitle:base::SysUTF16ToNSString(webState->GetTitle())
          tabID:webState->GetUniqueIdentifier().identifier()];
  [self.debugLogger logEvent:event];
}

- (void)webStateDeferredExecutor:(WebStateDeferredExecutor*)executor
         didForceRealizeWebState:(web::WebState*)webState {
  ComposeboxDebuggerEvent* event = [ComposeboxDebuggerEvent
       tabEvent:composebox_debugger::event::Tabs::kDidRealizeTab
      withTitle:base::SysUTF16ToNSString(webState->GetTitle())
          tabID:webState->GetUniqueIdentifier().identifier()];
  [self.debugLogger logEvent:event];
}

@end
