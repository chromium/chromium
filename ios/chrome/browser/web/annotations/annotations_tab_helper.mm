// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/annotations/annotations_tab_helper.h"

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/uuid.h"
#import "base/values.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/text_selection/model/text_classifier_model_service.h"
#import "ios/chrome/browser/text_selection/model/text_classifier_model_service_factory.h"
#import "ios/public/provider/chrome/browser/context_menu/context_menu_api.h"
#import "ios/web/common/annotations_utils.h"
#import "ios/web/common/features.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/annotations/annotations_text_manager.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

AnnotationsTabHelper::AnnotationsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  // In some cases, AnnotationsTextManager is created before this and in some
  // others after. Make sure it exists.
  web::AnnotationsTextManager::CreateForWebState(web_state);
  auto* manager = web::AnnotationsTextManager::FromWebState(web_state);
  manager->AddObserver(this);
}

AnnotationsTabHelper::~AnnotationsTabHelper() {
  web_state_ = nullptr;
}

void AnnotationsTabHelper::SetBaseViewController(
    UIViewController* base_view_controller) {
  base_view_controller_ = base_view_controller;
}

void AnnotationsTabHelper::SetMiniMapCommands(
    id<MiniMapCommands> mini_map_handler) {
  mini_map_handler_ = mini_map_handler;
}

void AnnotationsTabHelper::SetParcelTrackingOptInCommands(
    id<ParcelTrackingOptInCommands> parcel_tracking_handler) {
  parcel_tracking_handler_ = parcel_tracking_handler;
}

void AnnotationsTabHelper::SetUnitConversionCommands(
    id<UnitConversionCommands> unit_conversion_handler) {
  unit_conversion_handler_ = unit_conversion_handler;
}

#pragma mark - WebStateObserver methods.

void AnnotationsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_->RemoveObserver(this);
  auto* manager = web::AnnotationsTextManager::FromWebState(web_state);
  manager->RemoveObserver(this);
  web_state_ = nullptr;
}

void AnnotationsTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_, web_state);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    match_cache_.clear();
  }
}

#pragma mark - AnnotationsTextObserver methods.

void AnnotationsTabHelper::OnTextExtracted(web::WebState* web_state,
                                           const std::string& text,
                                           int seq_id,
                                           const base::Value::Dict& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(web_state_, web_state);

  // Check if this page requested "nointentdetection".
  std::optional<bool> has_no_intent_detection =
      metadata.FindBool("hasNoIntentDetection");
  if (!has_no_intent_detection || has_no_intent_detection.value()) {
    return;
  }

  // Keep latest copy.
  metadata_ = std::make_unique<base::Value::Dict>(metadata.Clone());

  TextClassifierModelService* service =
      TextClassifierModelServiceFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state->GetBrowserState()));
  base::FilePath model_path =
      service ? service->GetModelPath() : base::FilePath();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ios::provider::ExtractTextAnnotationFromText,
                     metadata.Clone(), text,
                     ios::provider::GetHandledIntentTypesForOneTap(web_state),
                     ukm::GetSourceIdForWebStateDocument(web_state),
                     std::move(model_path)),
      base::BindOnce(&AnnotationsTabHelper::ApplyDeferredProcessing,
                     weak_factory_.GetWeakPtr(), seq_id));
}

void AnnotationsTabHelper::OnDecorated(web::WebState* web_state,
                                       int successes,
                                       int annotations) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (annotations) {
    int percentage = (100 * successes) / annotations;
    base::UmaHistogramPercentage("IOS.Annotations.Percentage", percentage);
  }
}

void AnnotationsTabHelper::OnClick(web::WebState* web_state,
                                   const std::string& text,
                                   CGRect rect,
                                   const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (match_cache_.find(data) == match_cache_.end()) {
    return;
  }
  NSTextCheckingResult* match = match_cache_.at(data);
  auto* manager = web::AnnotationsTextManager::FromWebState(web_state_);
  if (manager) {
    manager->RemoveHighlight();
  }

  NSString* ns_text = base::SysUTF8ToNSString(text);
  const BOOL success = ios::provider::HandleIntentTypesForOneTap(
      web_state, match, ns_text, rect.origin, base_view_controller_,
      mini_map_handler_, unit_conversion_handler_);
  DCHECK(success);
}

#pragma mark - Private Methods

void AnnotationsTabHelper::ApplyDeferredProcessing(
    int seq_id,
    std::optional<std::vector<web::TextAnnotation>> deferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  web::ContentWorld content_world =
      web::AnnotationsTextManager::GetFeatureContentWorld();
  web::WebFrame* main_frame =
      web_state_->GetWebFramesManager(content_world)->GetMainWebFrame();
  if (main_frame && deferred) {
    auto* manager = web::AnnotationsTextManager::FromWebState(web_state_);
    DCHECK(manager);
    std::vector<web::TextAnnotation> annotations(std::move(deferred.value()));
    if ((IsIOSParcelTrackingEnabled() &&
         !IsParcelTrackingDisabled(GetApplicationContext()->GetLocalState())) ||
        base::FeatureList::IsEnabled(web::features::kEnableMeasurements)) {
      AnnotationsTabHelper::ProcessAnnotations(annotations);
    }
    base::Value::List decorations_list;
    BuildCacheAndDecorations(annotations, decorations_list);
    base::Value decorations(std::move(decorations_list));
    manager->DecorateAnnotations(web_state_, decorations, seq_id);
  }
}

void AnnotationsTabHelper::BuildCacheAndDecorations(
    std::vector<web::TextAnnotation>& annotations_list,
    base::Value::List& decorations) {
  for (web::TextAnnotation& data : annotations_list) {
    const std::string key = base::Uuid::GenerateRandomV4().AsLowercaseString();
    match_cache_[key] = data.second;
    data.first.Set("data", key);
    decorations.Append(base::Value(std::move(data.first)));
  }
}

void AnnotationsTabHelper::ProcessAnnotations(
    std::vector<web::TextAnnotation>& annotations_list) {
  NSMutableArray<CustomTextCheckingResult*>* unique_parcels =
      [[NSMutableArray alloc] init];
  NSMutableSet* existing_parcel_numbers = [NSMutableSet set];
  int detected_measurements = 0;
  for (auto annotation = annotations_list.begin();
       annotation != annotations_list.end();) {
    NSTextCheckingResult* match = annotation->second;
    if (!match || (match.resultType != TCTextCheckingTypeParcelTracking &&
                   match.resultType != TCTextCheckingTypeMeasurement)) {
      annotation++;
      continue;
    }
    CustomTextCheckingResult* custom_match =
        static_cast<CustomTextCheckingResult*>(match);

    if (match.resultType == TCTextCheckingTypeParcelTracking) {
      // Avoid adding duplicates to `unique_parcels`.
      if (![existing_parcel_numbers
              containsObject:[custom_match carrierNumber]]) {
        [existing_parcel_numbers addObject:[custom_match carrierNumber]];
        [unique_parcels addObject:custom_match];
      }
      // Remove the parcel from annotations_list to prevent decorating the
      // tracking number.
      annotation = annotations_list.erase(annotation);
      continue;
    } else if (match.resultType == TCTextCheckingTypeMeasurement) {
      detected_measurements++;
    }
    annotation++;
  }

  if (base::FeatureList::IsEnabled(web::features::kEnableMeasurements)) {
    base::UmaHistogramCounts100("IOS.UnitConversion.DetectedMeasurements",
                                detected_measurements);
  }

  // Show UI only if this is the currently active WebState.
  if ([unique_parcels count] > 0 && web_state_->IsVisible()) {
    DCHECK(IsIOSParcelTrackingEnabled() &&
           !IsParcelTrackingDisabled(GetApplicationContext()->GetLocalState()));
    // Call asynchronously to allow the rest of the annotations to be decorated
    // first.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AnnotationsTabHelper::MaybeShowParcelTrackingUI,
                       weak_factory_.GetWeakPtr(), unique_parcels));
  }
}

void AnnotationsTabHelper::MaybeShowParcelTrackingUI(
    NSArray<CustomTextCheckingResult*>* parcels) {
  [parcel_tracking_handler_ showTrackingForParcels:parcels];
}

WEB_STATE_USER_DATA_KEY_IMPL(AnnotationsTabHelper)
