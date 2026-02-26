// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_mediator.h"

#import <PDFKit/PDFKit.h>

#import <memory>
#import <queue>
#import <set>
#import <unordered_map>
#import <unordered_set>
#import <utility>

#import "base/apple/foundation_util.h"
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
#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/contextual_search/input_state_model.h"
#import "components/lens/contextual_input.h"
#import "components/lens/lens_bitmap_processing.h"
#import "components/lens/lens_url_utils.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/browser/lens_suggest_inputs_utils.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_url_loader.h"
#import "ios/chrome/browser/composebox/coordinator/web_state_deferred_executor.h"
#import "ios/chrome/browser/composebox/debugger/composebox_debugger_logger.h"
#import "ios/chrome/browser/composebox/public/composebox_constants.h"
#import "ios/chrome/browser/composebox/public/composebox_input_plate_controls.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_collection.h"
#import "ios/chrome/browser/composebox/ui/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/ui/composebox_server_strings.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
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

// Returns the model option required by the given model mode.
ComposeboxModelOption ModelOptionForModelMode(omnibox::ModelMode model_mode) {
  using enum ComposeboxModelOption;
  switch (model_mode) {
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return ComposeboxModelOption::kAuto;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
      return ComposeboxModelOption::kThinking;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
    default:
      return ComposeboxModelOption::kRegular;
  }
}

// Returns the input plate control for the given tool mode.
ComposeboxInputPlateControls InputPlateControlForToolMode(
    omnibox::ToolMode tool_mode) {
  switch (tool_mode) {
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return ComposeboxInputPlateControls::kCanvas;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return ComposeboxInputPlateControls::kDeepSearch;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD:
      return ComposeboxInputPlateControls::kCreateImage;
    case omnibox::ToolMode::TOOL_MODE_UNSPECIFIED:
    default:
      return ComposeboxInputPlateControls::kNone;
  }
}

// Returns the server strings object from a given input state.
ComposeboxServerStrings* ServerStringsFromInputState(
    const contextual_search::InputState& input_state) {
  std::unordered_map<ComposeboxInputPlateControls,
                     ComposeboxServerStringBundle*>
      tool_mapping;
  for (const omnibox::ToolConfig& tool_config : input_state.tool_configs) {
    NSString* menuLabel = base::SysUTF8ToNSString(tool_config.menu_label());
    NSString* chipLabel = base::SysUTF8ToNSString(tool_config.chip_label());
    NSString* hintText = base::SysUTF8ToNSString(tool_config.hint_text());
    tool_mapping[InputPlateControlForToolMode(tool_config.tool())] =
        [[ComposeboxServerStringBundle alloc] initWithMenuLabel:menuLabel
                                                      chipLabel:chipLabel
                                                       hintText:hintText];
  }

  std::unordered_map<ComposeboxModelOption, ComposeboxServerStringBundle*>
      model_mapping;
  for (const omnibox::ModelConfig& model_config : input_state.model_configs) {
    NSString* menuLabel = base::SysUTF8ToNSString(model_config.menu_label());
    NSString* hintText = base::SysUTF8ToNSString(model_config.hint_text());
    model_mapping[ModelOptionForModelMode(model_config.model())] =
        [[ComposeboxServerStringBundle alloc] initWithMenuLabel:menuLabel
                                                      chipLabel:nil
                                                       hintText:hintText];
  }

  NSString* modelSectionHeader = @"";
  NSString* toolsSectionHeader = @"";

  if (input_state.model_section_config) {
    modelSectionHeader =
        base::SysUTF8ToNSString(input_state.model_section_config->header());
  }

  if (input_state.tools_section_config) {
    toolsSectionHeader =
        base::SysUTF8ToNSString(input_state.tools_section_config->header());
  }

  return
      [[ComposeboxServerStrings alloc] initWithToolMapping:tool_mapping
                                              modelMapping:model_mapping
                                        modelSectionHeader:modelSectionHeader
                                        toolsSectionHeader:toolsSectionHeader];
}

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

}  // namespace

@interface ComposeboxInputPlateMediator () <
    SearchEngineObserving,
    ComposeboxInputItemCollectionDelegate,
    WebStateDeferredExecutorDelegate>
@end

@implementation ComposeboxInputPlateMediator {
  // The ordered list of items for display.
  ComposeboxInputItemCollection* _items;
  // The C++ session handle for this feature.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      _contextualSearchSession;
  // The observer bridge for file upload status.
  std::unique_ptr<ComposeboxFileUploadObserverBridge> _composeboxObserverBridge;
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

  // Stores the page context wrappers for the duration of the APC retrieval.
  std::unordered_map<web::WebStateID, PageContextWrapper*> _pageContextWrappers;
  std::unordered_map<base::UnguessableToken,
                     web::WebStateID,
                     base::UnguessableTokenHash>
      _latestTabSelectionMapping;

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
  // Whether it is in compact mode.
  BOOL _compact;
  // Whether the omnibox has text inputted.
  BOOL _hasText;
  // Whether a successful navigation has started.
  BOOL _inNavigation;
  // Used to count the number of images added in the session.
  int _imageUploadCount;
  // The currrent choice of model.
  ComposeboxModelOption _modelOption;

  // The state reflecting the availbale modes and models.
  std::unique_ptr<contextual_search::InputStateModel> _inputStateModel;
  contextual_search::InputState _inputState;
  // The subscription for updates on the input state.
  base::CallbackListSubscription _inputStateSubscription;
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
                        prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _contextualSearchSession = std::move(contextualSearchSession);
    _contextualSearchSession->NotifySessionStarted();
    CHECK(_contextualSearchSession->GetController());
    _composeboxObserverBridge =
        std::make_unique<ComposeboxFileUploadObserverBridge>(
            self, _contextualSearchSession->GetController());
    _webStateList = webStateList;
    _faviconLoader = faviconLoader;
    _webStateDeferredExecutor = [[WebStateDeferredExecutor alloc] init];
    _webStateDeferredExecutor.delegate = self;
    _persistTabContextAgent = persistTabContextAgent;
    _isIncognito = isIncognito;
    _modeHolder = modeHolder;
    [_modeHolder addObserver:self];
    _templateURLService = templateURLService;
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _aimEligibilityService = aimEligibilityService;
    _items = [[ComposeboxInputItemCollection alloc] init];
    _items.delegate = self;
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self recordNavigationResult];
  [_modeHolder removeObserver:self];
  _modeHolder = nil;
  _faviconLoader = nullptr;
  _webStateDeferredExecutor = nil;
  _persistTabContextAgent = nullptr;
  _searchEngineObserver.reset();
  _templateURLService = nullptr;
  [self invalidateInputStateSubscription];
  _aimEligibilityService = nullptr;
  _inputStateModel = nullptr;
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
}

- (void)setConsumer:(id<ComposeboxInputPlateConsumer>)consumer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _consumer = consumer;

  if (!_webStateList || !consumer) {
    return;
  }

  BOOL canAttachCurrentTab = [self updateOptionToAttachCurrentTab];

  if (canAttachCurrentTab) {
    [self extractFaviconForCurrentTab];
  }

  [self commitUIUpdates];
}

- (BOOL)canAddMoreAttachments {
  return [self remainingAttachmentCapacity] > 0;
}

// The absolute value for the maximum number of attachments available,
// regardless the type.
- (NSUInteger)totalAttachmentLimit {
  if (EnableComposeboxServerSideState()) {
    return _inputState.max_total_inputs;
  }

  return kAttachmentLimit;
}

- (NSUInteger)remainingAttachmentCapacity {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  NSUInteger availableSlots = [self totalAttachmentLimit] - _items.count;
  switch (_modeHolder.mode) {
    case ComposeboxMode::kRegularSearch:
    case ComposeboxMode::kCanvas:
    case ComposeboxMode::kAIM:
    // TODO(crbug.com/481280186): Check deep search attachment limtitation.
    case ComposeboxMode::kDeepSearch: {
      // For Regular search, canvas & AIM allow up to kAttachmentLimit items.
      return availableSlots;
    }
    case ComposeboxMode::kImageGeneration: {
      // For ImageGeneration, allow 1 image if no images are present, otherwise
      // 0.
      return _items.hasImage
                 ? 0
                 : MIN(availableSlots, kAttachmentLimitForImageGeneration);
    }
  }
}

- (NSUInteger)remainingNumberOfImagesAllowed {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  int remainingAttachmentCapacity = [self remainingAttachmentCapacity];
  if (EnableComposeboxServerSideState()) {
    CHECK(_inputStateModel);
    auto limits = _inputState.max_instances;
    auto type = omnibox::InputType::INPUT_TYPE_LENS_IMAGE;
    if (limits.count(type)) {
      int serverLimit = limits[type];
      int remainingSlots = serverLimit - _items.imagesCount;
      return MIN(remainingSlots, remainingAttachmentCapacity);
    }
  }

  return remainingAttachmentCapacity;
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
  auto advancedToolsParams = _inputStateModel->GetAdditionalQueryParams();
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

  _contextualSearchSession->CreateSearchUrl(std::move(search_url_request_info),
                                            std::move(callback));
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
                                              : nullptr];
}

- (void)processText:(NSString*)text {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate refineWithText:text];
}

- (void)processPDFFileURL:(GURL)PDFFileURL {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSString* assetID = base::SysUTF8ToNSString(PDFFileURL.spec());
  if ([_items assetAlreadyLoaded:assetID]) {
    return;
  }

  // Check file size.
  NSURL* nsURL = net::NSURLWithGURL(PDFFileURL);
  NSError* error = nil;
  NSNumber* fileSize =
      [[nsURL resourceValuesForKeys:@[ NSURLFileSizeKey ]
                              error:&error] objectForKey:NSURLFileSizeKey];
  if (fileSize && [fileSize unsignedLongLongValue] > kMaxPDFFileSize) {
    [self.delegate showSnackbarForItemUploadDidFail];
    return;
  }

  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeFile
                              assetID:assetID];
  item.title = base::SysUTF8ToNSString(PDFFileURL.ExtractFileName());
  [self addItem:item];
  base::UnguessableToken identifier = item.identifier;

  // Read the data in the background then call `onDataReadForItem`.
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadDataFromURL, PDFFileURL),
      base::BindOnce(^(NSData* data) {
        [weakSelf onDataReadForItemWithIdentifier:identifier
                                          fromURL:PDFFileURL
                                         withData:data];
      }));
}

- (void)processImageItemProvider:(NSItemProvider*)itemProvider
                         assetID:(NSString*)assetID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  BOOL unableToLoadUIImage =
      ![itemProvider canLoadObjectOfClass:[UIImage class]];

  BOOL assetAlreadyLoaded = [_items assetAlreadyLoaded:assetID];
  if (unableToLoadUIImage || assetAlreadyLoaded) {
    return;
  }

  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeImage
                              assetID:assetID];
  [self addItem:item];
  __block base::UnguessableToken identifier = item.identifier;

  __weak __typeof(self) weakSelf = self;
  // Load the preview image.
  [itemProvider
      loadPreviewImageWithOptions:nil
                completionHandler:^(UIImage* previewImage, NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadPreviewImage:previewImage
                            forItemWithIdentifier:identifier];
                  });
                }];

  // Concurrently load the full image.
  [itemProvider loadObjectOfClass:[UIImage class]
                completionHandler:^(__kindof id<NSItemProviderReading> object,
                                    NSError* error) {
                  dispatch_async(dispatch_get_main_queue(), ^{
                    [weakSelf didLoadFullImage:(UIImage*)object
                         forItemWithIdentifier:identifier];
                  });
                }];
}

- (void)setModelOption:(ComposeboxModelOption)modelOption {
  [self setModelOption:modelOption explicitUserAction:NO];
}

- (void)setModelOption:(ComposeboxModelOption)modelOption
    explicitUserAction:(BOOL)explicitUserAction {
  using enum ComposeboxModelOption;

  if (_modelOption == modelOption) {
    return;
  }

  _modelOption = modelOption;

  [self updateModel];

  if (_inputStateModel) {
    switch (modelOption) {
      case kNone:
        _inputStateModel->setActiveModel(_inputState.GetDefaultModel());
        break;
      case kRegular:
        _inputStateModel->setActiveModel(
            omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
        break;
      case kAuto:
        _inputStateModel->setActiveModel(
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
        break;
      case kThinking:
        _inputStateModel->setActiveModel(
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
        break;
      default:
        break;
    }
  }

  // When the model option is reset (set to none), reset the mode to regular
  // search before exiting.
  BOOL switchToRegular = _modelOption == kNone && !_modeHolder.isRegularSearch;
  if (switchToRegular) {
    _modeHolder.mode = ComposeboxMode::kRegularSearch;
    return;
  }
  // Only when the user explicitly picked the advanced model in regular mode
  // do the switch to AIM.
  BOOL switchToAIM = explicitUserAction && _modeHolder.isRegularSearch;
  if (switchToAIM) {
    _modeHolder.mode = ComposeboxMode::kAIM;
    return;
  }
}

- (void)setSearchboxConfig:(const omnibox::SearchboxConfig*)searchboxConfig {
  // Only preselect when there was already a input state model created.
  // Otherwise it's safe to assume it is the first time a searchbox config is
  // loaded.
  BOOL needPreselection = _inputStateModel != nil;

  contextual_search::InputState previousInputState = _inputState;

  contextual_search::ContextualSearchSessionHandle* sessionHandle =
      _contextualSearchSession.get();
  _inputStateModel = std::make_unique<contextual_search::InputStateModel>(
      *sessionHandle, *searchboxConfig, _isIncognito);

  if (needPreselection) {
    // Try maintaining the same options if there was no change in their
    // availability.
    __weak __typeof(self) weakSelf = self;
    [self preselectPreferencesIfAvailable:previousInputState
                               completion:^{
                                 [weakSelf startInputStateObservation];
                               }];
  } else {
    [self startInputStateObservation];
  }

  [self commitUIUpdates];
}

- (void)changeModeForInputState:
    (const contextual_search::InputState&)inputState {
  using enum ComposeboxMode;
  switch (inputState.active_tool) {
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      _modeHolder.mode = ComposeboxMode::kCanvas;
      return;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      _modeHolder.mode = ComposeboxMode::kDeepSearch;
      return;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD:
      _modeHolder.mode = ComposeboxMode::kImageGeneration;
      return;
    case omnibox::ToolMode::TOOL_MODE_UNSPECIFIED:
    default:
      if (_modeHolder.mode == ComposeboxMode::kAIM ||
          _modeHolder.isRegularSearch) {
        return;
      }
      _modeHolder.mode = ComposeboxMode::kRegularSearch;
  }
}

#pragma mark - ComposeboxModeObserver

- (void)composeboxModeDidChange:(ComposeboxMode)mode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  [self updateMode];

  switch (mode) {
    case ComposeboxMode::kRegularSearch:
      if (_contextualSearchSession) {
        _contextualSearchSession->ClearFiles();
      }
      [_items clearItems];
      _imageUploadCount = 0;
      [self setActiveTool:omnibox::TOOL_MODE_UNSPECIFIED];
      break;
    case ComposeboxMode::kAIM:
      if (![self isEligibleToAIM]) {
        _modeHolder.mode = ComposeboxMode::kRegularSearch;
      }
      [self setActiveTool:omnibox::TOOL_MODE_UNSPECIFIED];
      break;
    case ComposeboxMode::kImageGeneration:
      if (![self imageToolAllowed]) {
        _modeHolder.mode = ComposeboxMode::kRegularSearch;
      }
      [self cleanAttachmentsForImageGeneration];
      [self updateImageGenerationToolMode];
      break;
    case ComposeboxMode::kCanvas:
      if (![self canvasToolAllowed]) {
        _modeHolder.mode = ComposeboxMode::kRegularSearch;
      }
      [self setActiveTool:omnibox::TOOL_MODE_CANVAS];
      break;
    case ComposeboxMode::kDeepSearch:
      if (![self deepSearchToolAllowed]) {
        _modeHolder.mode = ComposeboxMode::kRegularSearch;
      }
      [self setActiveTool:omnibox::TOOL_MODE_DEEP_SEARCH];
      break;
  }

  [self updateModelOnModeChange];
  [self commitUIUpdates];
  [self reloadSuggestions];
}

#pragma mark - ComposeboxTabPickerSelectionDelegate

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

- (NSUInteger)nonTabAttachmentCount {
  return _items.nonTabAttachmentCount;
}

- (NSUInteger)maxTabAttachmentCount {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  int remainingAttachmentCapacity = [self remainingAttachmentCapacity];
  int tabsCount = _items.tabsCount;
  int capacityForTabs = remainingAttachmentCapacity + tabsCount;

  if (EnableComposeboxServerSideState()) {
    CHECK(_inputStateModel);
    auto limits = _inputState.max_instances;
    auto type = omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
    if (limits.count(type)) {
      int serverLimit = limits[type];
      return MIN(serverLimit, capacityForTabs);
    }
  }

  return capacityForTabs;
}

- (void)attachSelectedTabsWithWebStateIDs:
            (std::set<web::WebStateID>)selectedWebStateIDs
                        cachedWebStateIDs:
                            (std::set<web::WebStateID>)cachedWebStateIDs {
  [self attachSelectedTabsWithWebStateIDs:selectedWebStateIDs
                        cachedWebStateIDs:cachedWebStateIDs
                     fromExternalWebState:nullptr];
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
- (base::UnguessableToken)createInputItemForWebState:(web::WebState*)webState {
  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeTab];
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
  [pageContextWrapper populatePageContextFieldsAsync];

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

  // TODO(crbug.com/40280872): Plumb encoding options from a central config.
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;

  if (_contextualSearchSession) {
    _contextualSearchSession->StartTabContextUploadFlow(
        serverToken, std::move(inputData), image_options);
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
      [weakSelf.consumer setCurrentTabFavicon:attributes.faviconImage];
    }
  };

  _faviconLoader->FaviconForPageUrl(
      webState->GetVisibleURL(), gfx::kFaviconSize, gfx::kFaviconSize,
      /*fallback_to_google_server=*/true, faviconLoadedBlock);
}

- (void)attachCurrentTabContent {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (![self canAddMoreAttachments]) {
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
  [self attachSelectedTabsWithWebStateIDs:webStateIDs cachedWebStateIDs:{}];
}

- (void)requestUIRefresh {
  [self commitUIUpdates];
}

#pragma mark - ComposeboxFileUploadObserver

- (void)onFileUploadStatusChanged:(const base::UnguessableToken&)fileToken
                         mimeType:(lens::MimeType)mimeType
                 fileUploadStatus:
                     (contextual_search::FileUploadStatus)fileUploadStatus
                        errorType:(const std::optional<
                                      contextual_search::FileUploadErrorType>&)
                                      errorType {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ComposeboxInputItem* item = [_items itemForServerToken:fileToken];
  if (!item) {
    return;
  }

  switch (fileUploadStatus) {
    case contextual_search::FileUploadStatus::kUploadSuccessful:
      [self setState:ComposeboxInputItemState::kLoaded onItem:item];
      break;
    case contextual_search::FileUploadStatus::kUploadFailed:
    case contextual_search::FileUploadStatus::kValidationFailed:
    case contextual_search::FileUploadStatus::kUploadExpired:
      [self handleFailedAttachment:item.identifier];
      break;
    case contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady:
      [self reloadSuggestions];
      break;
    case contextual_search::FileUploadStatus::kNotUploaded:
    case contextual_search::FileUploadStatus::kProcessing:
    case contextual_search::FileUploadStatus::kUploadStarted:
    case contextual_search::FileUploadStatus::kUploadReplaced:
      // No-op, as the state is already `Uploading`.
      return;
  }

  [self.consumer updateState:item.state forItemWithIdentifier:item.identifier];
}

#pragma mark - Private

// Updates the tool mode when in image generation mode.
- (void)updateImageGenerationToolMode {
  if (_modeHolder.mode != ComposeboxMode::kImageGeneration) {
    return;
  }

  BOOL imageGenUploadMode = _items.count > 0;

  omnibox::ToolMode toolMode =
      imageGenUploadMode ? omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD
                         : omnibox::ToolMode::TOOL_MODE_IMAGE_GEN;
  if (_inputState.active_tool != toolMode) {
    _inputStateModel->setActiveTool(toolMode);
  }
}

// Informs the model of a context change (e.g.; attachment added or deleted).
- (void)notifyContextChanged {
  if (_inputStateModel) {
    _inputStateModel->OnContextChanged();
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
    case ComposeboxInputItemType::kComposeboxInputItemTypeFile:
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
                     fromExternalWebState:(web::WebState*)externalWebState {
  [self.metricsRecorder recordTabPickerTabsAttached:selectedWebStateIDs.size()];

  _pageContextWrappers.clear();

  // Remove tabs from context that were deselected in the tab picker.
  std::set<web::WebStateID> alreadyProcessedIDsFromCurrentWebState =
      [self attachedWebStateIDsInCurrentContext];
  std::set<web::WebStateID> deselectedIDs;
  set_difference(alreadyProcessedIDsFromCurrentWebState.begin(),
                 alreadyProcessedIDsFromCurrentWebState.end(),
                 selectedWebStateIDs.begin(), selectedWebStateIDs.end(),
                 inserter(deselectedIDs, deselectedIDs.begin()));
  [self removeDeselectedIDs:deselectedIDs];

  // Prevent duplicate tabs from external web states from being added to
  // context.
  std::set<web::WebStateID> alreadyProcessedIDs = [self allAttachedWebStateIDs];
  std::set<web::WebStateID> newlyAddedIDs;
  set_difference(selectedWebStateIDs.begin(), selectedWebStateIDs.end(),
                 alreadyProcessedIDs.begin(), alreadyProcessedIDs.end(),
                 inserter(newlyAddedIDs, newlyAddedIDs.begin()));

  if (newlyAddedIDs.empty()) {
    return;
  }

  for (const web::WebStateID& candidateID : newlyAddedIDs) {
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
        [self createInputItemForWebState:candidateWebState];
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

- (void)didCreateSearchURL:(GURL)URL {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // TODO(crbug.com/40280872): Handle AIM enabled in the query controller.
  if ([_modeHolder isRegularSearch]) {
    URL = net::AppendOrReplaceQueryParameter(URL, "udm", "24");
  }

  UrlLoadParams params = CreateOmniboxUrlLoadParams(
      URL, /*post_content=*/nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_GENERATED,
      /*destination_url_entered_without_scheme=*/false, _isIncognito);

  _inNavigation = YES;

  [self.URLLoader loadURLParams:params];
}

// Records whether the session resulted in navigation.
- (void)recordNavigationResult {
  switch (_modeHolder.mode) {
    case ComposeboxMode::kRegularSearch:
      [self.metricsRecorder
          recordComposeboxFocusResultedInNavigation:_inNavigation
                                    withAttachments:!_items.empty
                                        requestType:AutocompleteRequestType::
                                                        kSearch];
      break;
    case ComposeboxMode::kAIM:
      [self.metricsRecorder
          recordComposeboxFocusResultedInNavigation:_inNavigation
                                    withAttachments:!_items.empty
                                        requestType:AutocompleteRequestType::
                                                        kAIMode];
      break;
    case ComposeboxMode::kImageGeneration:
      [self.metricsRecorder
          recordComposeboxFocusResultedInNavigation:_inNavigation
                                    withAttachments:!_items.empty
                                        requestType:AutocompleteRequestType::
                                                        kImageGeneration];
      break;
    case ComposeboxMode::kCanvas:
      [self.metricsRecorder
          recordComposeboxFocusResultedInNavigation:_inNavigation
                                    withAttachments:!_items.empty
                                        requestType:AutocompleteRequestType::
                                                        kCanvas];
      break;
    case ComposeboxMode::kDeepSearch:
      [self.metricsRecorder
          recordComposeboxFocusResultedInNavigation:_inNavigation
                                    withAttachments:!_items.empty
                                        requestType:AutocompleteRequestType::
                                                        kImageGeneration];
      break;
  }
}

// Reloads the displayed suggestions based on the attachments/modeHolder.
- (void)reloadSuggestions {
  BOOL shouldRestartAutocomplete = _items.count <= 1;

  if (_items.count > 1) {
    shouldRestartAutocomplete =
        IsComposeboxFetchContextualSuggestionsForMultiAttachmentsEnabled();
  }
  [self.delegate
      reloadAutocompleteSuggestionsRestarting:shouldRestartAutocomplete];
}

// Cleans attachments when switching to image generation mode.
// This method ensures that only one image attachment is kept, and all other
// attachments (including other images, tabs, and files) are removed.
- (void)cleanAttachmentsForImageGeneration {
  NSMutableArray<ComposeboxInputItem*>* itemsToKeep = [NSMutableArray array];
  ComposeboxInputItem* imageToKeep = nil;

  // Find one image to keep.
  for (ComposeboxInputItem* item in _items.containedItems) {
    if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeImage &&
        !imageToKeep) {
      imageToKeep = item;
      [itemsToKeep addObject:item];
      break;
    }
  }

  if (itemsToKeep.count == _items.count) {
    // No items were removed.
    return;
  }

  // Find items to remove from the backend.
  for (ComposeboxInputItem* item in _items.containedItems) {
    if (![itemsToKeep containsObject:item]) {
      if (_contextualSearchSession) {
        _contextualSearchSession->DeleteFile(item.serverToken);
      }
    }
  }

  [_items replaceWithItems:itemsToKeep];

  [self notifyContextChanged];
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
      [weakSelf onFileUploadStatusChanged:identifier
                                 mimeType:lens::MimeType::kImage
                         fileUploadStatus:contextual_search::FileUploadStatus::
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

  // TODO(crbug.com/40280872): Plumb encoding options from a central config.
  lens::ImageEncodingOptions image_options;
  image_options.max_width = 1024;
  image_options.max_height = 1024;
  image_options.compression_quality = 80;

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
                                    options:image_options];
      }));
}

// Handles uploading the context after the snapshot is generated.
- (void)didRetrieveColorSnapshot:(UIImage*)image
                       inputData:(std::unique_ptr<lens::ContextualInputData>)
                                     input_data
                      identifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (image) {
    NSData* data = UIImagePNGRepresentation(image);
    std::vector<uint8_t> image_vector_data([data length]);
    [data getBytes:image_vector_data.data() length:[data length]];
    input_data->viewport_screenshot_bytes = std::move(image_vector_data);
  }
  [self didLoadPreviewImage:image forItemWithIdentifier:identifier];

  [self uploadTabForIdentifier:identifier inputData:std::move(input_data)];
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
    _contextualSearchSession->StartFileContextUploadFlow(
        serverToken, fileName, kAdobePortableDocumentFormatMimeType,
        std::move(buffer),
        /*image_options=*/std::nullopt);
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

// Checks whether the user is eligibile to share content (enterprise policy).
- (BOOL)isContentSharingEnabled {
  return _prefService && _contextualSearchSession &&
         _contextualSearchSession->CheckSearchContentSharingSettings(
             _prefService);
}

// Checks if the user is eligible for AIM, taking into account experimental
// settings overrides.
- (BOOL)isEligibleToAIM {
  if (experimental_flags::ShouldForceDisableComposeboxAIM()) {
    return NO;
  }
  if (IsComposeboxAIMDisabled()) {
    return NO;
  }
  if (!_aimEligibilityService) {
    return NO;
  }
  return _aimEligibilityService->IsAimEligible();
}

// Checks if the user is allowed to create images, taking into account
// eligibility and experimental settings overrides.
- (BOOL)imageToolAllowed {
  if (experimental_flags::ShouldForceDisableComposeboxCreateImages()) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return
        [self toolAllowedInInputState:omnibox::ToolMode::TOOL_MODE_IMAGE_GEN];
  } else {
    if (!_aimEligibilityService) {
      return NO;
    }
    return _aimEligibilityService->IsCreateImagesEligible();
  }
}

// Whether the client is allowed to access canvas mode.
- (BOOL)canvasToolAllowed {
  if (!ShowComposeboxAdditionalAdvancedTools()) {
    return NO;
  }
  if (experimental_flags::ShouldForceDisableComposeboxCanvas()) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return [self toolAllowedInInputState:omnibox::TOOL_MODE_CANVAS];
  } else {
    if (!_aimEligibilityService) {
      return NO;
    }
    return _aimEligibilityService->IsCanvasEligible();
  }
}

// Whether the client is allowed to access deep search mode.
- (BOOL)deepSearchToolAllowed {
  if (!ShowDeepSearchTool()) {
    return NO;
  }
  if (experimental_flags::ShouldForceDisableComposeboxDeepSearch()) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return [self toolAllowedInInputState:omnibox::TOOL_MODE_DEEP_SEARCH];
  } else {
    if (!_aimEligibilityService) {
      return NO;
    }
    return _aimEligibilityService->IsDeepSearchEligible();
  }
}

// Checks if the user is eligible to upload PDFs, taking into account
// experimental settings overrides.
- (BOOL)isEligibleToUploadPdf {
  if (experimental_flags::ShouldForceDisableComposeboxPdfUpload()) {
    return NO;
  }
  if (!_aimEligibilityService || ![self isContentSharingEnabled]) {
    return NO;
  }
  return _aimEligibilityService->IsPdfUploadEligible();
}

// Whether Create Image is in the list of disabled tools.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)imageToolDisabled {
  // Allow deselecting the mode.
  if (_modeHolder.mode == ComposeboxMode::kImageGeneration) {
    return NO;
  }
  BOOL generateImageDisabled =
      [self toolDisabledInInputState:omnibox::ToolMode::TOOL_MODE_IMAGE_GEN] ||
      [self toolDisabledInInputState:omnibox::ToolMode::
                                         TOOL_MODE_IMAGE_GEN_UPLOAD];
  BOOL hasTabOrFile = _items.hasTabOrFile;
  return generateImageDisabled || hasTabOrFile;
}

// Whether Canvas is in the list of disabled tools.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)canvasToolDisabled {
  // Allow deselecting the mode.
  if (_modeHolder.mode == ComposeboxMode::kCanvas) {
    return NO;
  }
  return [self toolDisabledInInputState:omnibox::ToolMode::TOOL_MODE_CANVAS];
}

// Whether Deep Search is in the list of disabled tools.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)deepSearchToolDisabled {
  // Allow deselecting the mode.
  if (_modeHolder.mode == ComposeboxMode::kDeepSearch) {
    return NO;
  }
  return
      [self toolDisabledInInputState:omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH];
}

#pragma mark - InputState rules helpers

// Whether the given mode is allowed in the input state.
- (BOOL)toolAllowedInInputState:(omnibox::ToolMode)toolMode {
  if (!EnableComposeboxServerSideState()) {
    return YES;
  }
  return std::find(_inputState.allowed_tools.begin(),
                   _inputState.allowed_tools.end(),
                   toolMode) != _inputState.allowed_tools.end();
}

// Whether the given mode is disabled in the input state.
- (BOOL)toolDisabledInInputState:(omnibox::ToolMode)toolMode {
  if (!EnableComposeboxServerSideState()) {
    return NO;
  }
  return std::find(_inputState.disabled_tools.begin(),
                   _inputState.disabled_tools.end(),
                   toolMode) != _inputState.disabled_tools.end();
}

// Whether the given model mode is selectable.
- (BOOL)canSelectToolBasedOnInputState:(omnibox::ToolMode)toolMode {
  return [self toolAllowedInInputState:toolMode] &&
         ![self toolDisabledInInputState:toolMode];
}

// Whether the given mode is allowed in the input state.
- (BOOL)modelAllowedInInputState:(omnibox::ModelMode)modelMode {
  if (!EnableComposeboxServerSideState()) {
    return YES;
  }
  return std::find(_inputState.allowed_models.begin(),
                   _inputState.allowed_models.end(),
                   modelMode) != _inputState.allowed_models.end();
}

// Whether the given mode is disabled in the input state.
- (BOOL)modelDisabledInInputState:(omnibox::ModelMode)modelMode {
  if (!EnableComposeboxServerSideState()) {
    return NO;
  }
  return std::find(_inputState.disabled_models.begin(),
                   _inputState.disabled_models.end(),
                   modelMode) != _inputState.disabled_models.end();
}

// Whether the given model mode is selectable.
- (BOOL)canSelectModelBasedOnInputState:(omnibox::ModelMode)modelMode {
  return [self modelAllowedInInputState:modelMode] &&
         ![self modelDisabledInInputState:modelMode];
}

// The list of model options available based on the input model.
- (std::unordered_set<ComposeboxModelOption>)allowedModels {
  std::unordered_set<ComposeboxModelOption> allowed = {};
  if (!ShowComposeboxAdditionalAdvancedTools()) {
    return allowed;
  }
  for (auto modelType : _inputState.allowed_models) {
    allowed.insert(ModelOptionForModelMode(modelType));
  }

  return allowed;
}

// The list of model options disabled based on the input model.
- (std::unordered_set<ComposeboxModelOption>)disabledModels {
  std::unordered_set<ComposeboxModelOption> disabled = {};
  if (!ShowComposeboxAdditionalAdvancedTools()) {
    return disabled;
  }
  for (auto modelType : _inputState.disabled_models) {
    disabled.insert(ModelOptionForModelMode(modelType));
  }

  return disabled;
}

#pragma mark - Attachments availability checks

// Whether the current input state disables the given input type.
- (BOOL)inputStateDisablesType:(omnibox::InputType)inputType {
  return std::find(_inputState.disabled_input_types.begin(),
                   _inputState.disabled_input_types.end(),
                   inputType) != _inputState.disabled_input_types.end();
}

// Whether the current input state allows the given input type.
- (BOOL)inputStateAllowsType:(omnibox::InputType)inputType {
  return std::find(_inputState.allowed_input_types.begin(),
                   _inputState.allowed_input_types.end(),
                   inputType) != _inputState.allowed_input_types.end();
}

// Whether the current state allows tab attachments.
- (BOOL)tabAttachmentAllowed {
  if (![self attachmentsAvailable]) {
    return NO;
  }
  if (EnableComposeboxServerSideState()) {
    return [self inputStateAllowsType:omnibox::INPUT_TYPE_BROWSER_TAB];
  }

  return YES;
}

// Whether the current state allows tab attachments.
- (BOOL)fileAttachmentAllowed {
  if (![self attachmentsAvailable]) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return [self inputStateAllowsType:omnibox::INPUT_TYPE_LENS_FILE];
  } else {
    return [self isEligibleToUploadPdf];
  }
}

// Whether the current state allows image attachments.
- (BOOL)imageAttachmentAllowed {
  if (![self attachmentsAvailable]) {
    return NO;
  }

  if (EnableComposeboxServerSideState() &&
      ![self inputStateAllowsType:omnibox::INPUT_TYPE_LENS_IMAGE]) {
    return NO;
  }

  return YES;
}

// Disables tab attachment.
- (BOOL)tabAttachmentDisabled {
  if (![self canAddMoreAttachments]) {
    return YES;
  }

  if (EnableComposeboxServerSideState()) {
    return [self inputStateDisablesType:omnibox::INPUT_TYPE_BROWSER_TAB];
  }

  BOOL isImageCreationMode =
      _modeHolder.mode == ComposeboxMode::kImageGeneration;
  return isImageCreationMode;
}

// Whether the current state allows tab attachments.
- (BOOL)fileAttachmentDisabled {
  if (![self canAddMoreAttachments]) {
    return YES;
  }

  if (EnableComposeboxServerSideState()) {
    return [self inputStateDisablesType:omnibox::INPUT_TYPE_LENS_FILE];
  }

  BOOL isImageCreationMode =
      _modeHolder.mode == ComposeboxMode::kImageGeneration;
  return isImageCreationMode;
}

// Whether the current state allows image attachments.
- (BOOL)imageAttachmentDisabled {
  if (![self canAddMoreAttachments]) {
    return YES;
  }

  if (EnableComposeboxServerSideState()) {
    return [self inputStateDisablesType:omnibox::INPUT_TYPE_LENS_IMAGE];
  }

  return NO;
}

- (BOOL)attachmentsAvailable {
  if (![self isContentSharingEnabled]) {
    return NO;
  }

  BOOL canSearchWithAI = [self isEligibleToAIM];
  BOOL canCreateImage = [self imageToolAllowed];
  BOOL canUseCanvas = [self canvasToolAllowed];
  BOOL canUseDeepSearch = [self deepSearchToolAllowed];
  return canUseCanvas || canCreateImage || canUseDeepSearch || canSearchWithAI;
}

- (BOOL)isDSEGoogle {
  if (!_templateURLService) {
    return NO;
  }
  return search::DefaultSearchProviderIsGoogle(_templateURLService);
}

- (BOOL)compactModeRequired {
  BOOL dseGoogle = [self isDSEGoogle];
  BOOL eligibleToAIM = [self isEligibleToAIM];
  BOOL allowsMultimodalActions = dseGoogle && eligibleToAIM;

  // If multimodal actions are disabled (e.g., when DSE is not Google), compact
  // mode is used to display the simpler input method, regardless the treatment.
  if (!allowsMultimodalActions) {
    return !_isMultiline;
  }

  if (!IsComposeboxCompactModeEnabled()) {
    return NO;
  }
  BOOL requiresExpansion =
      _isMultiline || _modeHolder.mode != ComposeboxMode::kRegularSearch;
  return !requiresExpansion;
}

#pragma mark - ComposeboxOmniboxClientDelegate

- (contextual_search::InputState)inputState {
  return _inputState;
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
  switch (_modeHolder.mode) {
    case ComposeboxMode::kRegularSearch:
      _inNavigation = YES;
      [self.URLLoader loadURLParams:URLLoadParams];
      [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                                AutocompleteRequestType::kSearch];
      break;
    case ComposeboxMode::kAIM:
      [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                                AutocompleteRequestType::kAIMode];
      if (IsAimURL(destinationURL)) {
        [self sendText:[NSString cr_fromString16:text]
            additionalParams:lens::GetParametersMapWithoutQuery(
                                 destinationURL)];
      } else {
        [self sendText:[NSString cr_fromString16:text]];
      }
      break;
    case ComposeboxMode::kImageGeneration:
      [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                                AutocompleteRequestType::kImageGeneration];
      [self sendText:[NSString cr_fromString16:text]];
      break;
    case ComposeboxMode::kCanvas:
      [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                                AutocompleteRequestType::kCanvas];
      [self sendText:[NSString cr_fromString16:text]];
      break;
    case ComposeboxMode::kDeepSearch:
      [self.metricsRecorder recordAutocompleteRequestTypeAtNavigation:
                                AutocompleteRequestType::kDeepSearch];
      [self sendText:[NSString cr_fromString16:text]];
      break;
  }
}

- (void)omniboxDidChangeText:(const std::u16string&)text
               isSearchQuery:(BOOL)isSearchQuery
         userInputInProgress:(BOOL)userInputInProgress {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  BOOL hasText = text.length() > 0;
  if (hasText == _hasText) {
    return;
  }
  _hasText = hasText;

  [self commitUIUpdates];
}

- (ComposeboxMode)composeboxMode {
  return _modeHolder.mode;
}

#pragma mark - Private helpers

// Whether the user can ask about the current Tab.
- (BOOL)canAskAboutCurrentTab {
  return IsAimCobrowseEnabled() && [self canAttachActiveTab];
}

// Whether the current tab is attachable.
- (BOOL)canAttachActiveTab {
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return NO;
  }

  std::set<web::WebStateID> alreadyProcessedIDs =
      [self attachedWebStateIDsInCurrentContext];
  BOOL isNTP = IsUrlNtp(webState->GetVisibleURL());
  BOOL alreadyProcessed =
      alreadyProcessedIDs.contains(webState->GetUniqueIdentifier());

  BOOL canAttachTab =
      !isNTP && !alreadyProcessed && [self isContentSharingEnabled];

  return canAttachTab;
}

// Reacts to a change in the model choice.
- (void)updateModelOnModeChange {
  using enum ComposeboxModelOption;

  if (_modeHolder.isRegularSearch) {
    [self setModelOption:kNone];
    return;
  }

  BOOL applyDefaultSelection = _modelOption == kNone;
  if (applyDefaultSelection) {
    auto allowedModels = [self allowedModels];
    auto disabledModel = [self disabledModels];
    BOOL autoAllowed = allowedModels.contains(kAuto);
    BOOL autoDisabled = disabledModel.contains(kAuto);
    BOOL defaultToAuto = autoAllowed && !autoDisabled;

    ComposeboxModelOption defaultOption = defaultToAuto
                                              ? ComposeboxModelOption::kAuto
                                              : ComposeboxModelOption::kRegular;

    [self setModelOption:defaultOption];
  }
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

- (void)updateButtonsVisibility {
  using enum ComposeboxInputPlateControls;
  BOOL compactMode = [self compactModeRequired];
  BOOL hasAttachments = !_items.empty;
  BOOL hasContent = hasAttachments || _hasText;
  BOOL dseGoogle = [self isDSEGoogle];
  BOOL eligibleToAIM = [self isEligibleToAIM];
  BOOL lensAvailable = lens_availability::CheckAvailabilityForLensEntryPoint(
      LensEntrypoint::Composebox, [self isDSEGoogle]);
  BOOL allowsMultimodalActions = dseGoogle && eligibleToAIM;
  BOOL canSend = hasContent && !compactMode && allowsMultimodalActions;
  BOOL showShortcuts =
      !hasContent && !canSend &&
      !base::FeatureList::IsEnabled(kHideFuseboxVoiceLensActions);
  BOOL showLeadingImage = !compactMode || !allowsMultimodalActions;
  BOOL shouldPersistAIMButton =
      IsComposeboxAIMNudgeEnabled() && !compactMode && allowsMultimodalActions;

  ComposeboxInputPlateControls leadingAction =
      allowsMultimodalActions ? kPlus : kNone;

  ComposeboxInputPlateControls leadingImage =
      showLeadingImage ? kLeadingImage : kNone;

  ComposeboxInputPlateControls modeSwitchButton;
  switch (_modeHolder.mode) {
    case ComposeboxMode::kAIM:
      modeSwitchButton = kAIM;
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

- (BOOL)updateOptionToAttachCurrentTab {
  BOOL canAttachTab = [self canAttachActiveTab];

  [_consumer hideAttachCurrentTabAction:!canAttachTab];

  return canAttachTab;
}

/// Updates the consumer actions enabled/disable state.
- (void)updateConsumerActionsState {
  // Image generation action.
  [self.consumer disableCreateImageActions:[self imageToolDisabled]];
  [self.consumer hideCreateImageActions:![self imageToolAllowed]];

  // Canvas action.
  [self.consumer disableCanvasActions:[self canvasToolDisabled]];
  [self.consumer hideCanvasActions:![self canvasToolAllowed]];

  // Deep search action.
  [self.consumer disableDeepSearchActions:[self deepSearchToolDisabled]];
  [self.consumer hideDeepSearchActions:![self deepSearchToolAllowed]];

  // Model picker.
  // TODO(crbug.com/477888273): Handle attachment incompatibility based on
  // server-side logic.
  [self.consumer allowModelPicker:ShowComposeboxAdditionalAdvancedTools()];
  [self.consumer setAllowedModels:[self allowedModels]];
  [self.consumer setDisabledModels:[self disabledModels]];

  // Add tabs action.
  [self.consumer disableAttachTabActions:[self tabAttachmentDisabled]];
  [self.consumer hideAttachTabActions:![self tabAttachmentAllowed]];

  // Add files action.
  [self.consumer disableAttachFileActions:[self fileAttachmentDisabled]];
  [self.consumer hideAttachFileActions:![self fileAttachmentAllowed]];

  // Add pictures from user gallery action.
  [self.consumer disableGalleryActions:[self imageAttachmentDisabled]];
  [self.consumer hideGalleryActions:![self imageAttachmentAllowed]];

  // Add picture from camera action.
  [self.consumer disableCameraActions:[self imageAttachmentDisabled]];
  [self.consumer hideCameraActions:![self imageAttachmentAllowed]];

  // Set the number of attachments that can still be added.
  [self.consumer
      setRemainingAttachmentCapacity:[self remainingAttachmentCapacity]];
}

/// Updates the consumer items and maybe trigger AIM.
- (void)updateConsumerItems {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.consumer setItems:_items.containedItems];
  [self updateOptionToAttachCurrentTab];

  // AI mode is implicitly enabled by items attachment.
  BOOL shouldSwitchToAIM = !_items.empty && [_modeHolder isRegularSearch];
  if (shouldSwitchToAIM) {
    [self.metricsRecorder
        recordAiModeActivationSource:AiModeActivationSource::kImplicit];
    _modeHolder.mode = ComposeboxMode::kAIM;
  }
}

// Updates the UI for the visible mode.
- (void)updateMode {
  auto mode = _modeHolder.mode;
  [self.consumer setAIModeEnabled:mode == ComposeboxMode::kAIM];
  [self.consumer
      setImageGenerationEnabled:mode == ComposeboxMode::kImageGeneration];
  [self.consumer setCanvasEnabled:mode == ComposeboxMode::kCanvas];
  [self.consumer setDeepSearchEnabled:mode == ComposeboxMode::kDeepSearch];
}

// Updates the UI for the current model.
- (void)updateModel {
  // In regular
  if (_modeHolder.isRegularSearch) {
    [_consumer setModelOption:ComposeboxModelOption::kNone];
    return;
  }

  [_consumer setModelOption:_modelOption];
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

// Pushes the batched UI updates to the consumer.
- (void)commitUIUpdates {
  _isUpdatingCompactMode = YES;

  // Update button visibility first, as the compact state change is asynchronous
  // and could conflict.
  [self updateButtonsVisibility];
  [self updateConsumerActionsState];
  [self updateCompactMode];
  [self updateModel];
  [self updateMode];

  _isUpdatingCompactMode = NO;
}

- (void)setActiveTool:(omnibox::ToolMode)activeTool {
  if (_inputStateModel) {
    _inputStateModel->setActiveTool(activeTool);
  }
}

#pragma mark - Input State Subscription

// Creates a new input state model based on the config from the AIM eligibility
// service.
- (void)createInputStateModel {
  const omnibox::SearchboxConfig* config =
      _aimEligibilityService->GetSearchboxConfig();
  contextual_search::ContextualSearchSessionHandle* sessionHandle =
      _contextualSearchSession.get();
  _inputStateModel = std::make_unique<contextual_search::InputStateModel>(
      *sessionHandle, *config, _isIncognito);
}

- (void)preselectPreferencesIfAvailable:
            (const contextual_search::InputState&)preselectionState
                             completion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  _inputStateSubscription = _inputStateModel->subscribe(
      base::BindRepeating(^(const contextual_search::InputState& inputState) {
        // Make sure the preselection sequence happens only once by invalidating
        // the subscription as soon as the initial input state is determined;
        // Otherwise subsequent updates will cause it to loop.
        [weakSelf invalidateInputStateSubscription];
        [weakSelf applyPreselection:preselectionState
                  forReferenceState:inputState];

        if (completion) {
          completion();
        }
      }));
  _inputStateModel->Initialize();
}

// Attempts to prepopulate the input state after an environment change.
// This is subject to restriction based on the reference state.
- (void)applyPreselection:
            (const contextual_search::InputState&)preselectionState
        forReferenceState:(const contextual_search::InputState&)referenceState {
  bool canSelectModel =
      [self canSelectModelBasedOnInputState:preselectionState.active_model];
  if (canSelectModel) {
    _inputStateModel->setActiveModel(preselectionState.active_model);
  }

  bool canSelectTool =
      [self canSelectToolBasedOnInputState:preselectionState.active_tool];
  if (canSelectTool) {
    [self setActiveTool:preselectionState.active_tool];
  }
}

- (void)invalidateInputStateSubscription {
  _inputStateSubscription = {};
}

// Starts observing changes in the input state. Emits the initial state
// immediately after starting.
- (void)startInputStateObservation {
  __weak __typeof(self) weakSelf = self;
  _inputStateSubscription = _inputStateModel->subscribe(
      base::BindRepeating(^(const contextual_search::InputState& inputState) {
        [weakSelf didUpdateInputState:inputState];
      }));
  _inputStateModel->Initialize();
}

// Called when the input state is updated.
- (void)didUpdateInputState:(contextual_search::InputState)inputState {
  _inputState = inputState;

  if (EnableComposeboxServerSideState()) {
    ComposeboxServerStrings* serverStrings =
        ServerStringsFromInputState(inputState);
    [self.consumer setServerStrings:serverStrings];
  }

  [self changeModeForInputState:inputState];
  if (!_modeHolder.isRegularSearch) {
    ComposeboxModelOption requiredModel =
        ModelOptionForModelMode(inputState.active_model);
    [self setModelOption:requiredModel];
  }

  [self commitUIUpdates];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
      LensEntrypoint::Composebox, [self isDSEGoogle]);
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
  [self updateImageGenerationToolMode];
}

#pragma mark - VoiceSearchDelegate

- (void)voiceSearchDidReceiveSearchQuery:(NSString*)query {
  [self sendText:query];
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
