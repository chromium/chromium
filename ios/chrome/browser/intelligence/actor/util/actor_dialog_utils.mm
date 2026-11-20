// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/util/actor_dialog_utils.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/web_state.h"

bool ShouldSuppressDialogDuringActuation(web::WebState* web_state,
                                         ActorSuppressedDialogType type) {
  if (!web_state) {
    return false;
  }

  ActorTabHelper* tab_helper = ActorTabHelper::FromWebState(web_state);
  // The actuation check must precede the feature check so that clients which
  // never run an actor task do not get enrolled in the experiment.
  if (!tab_helper || tab_helper->GetControlState() !=
                         actor::ActorControlState::kActorControlled) {
    return false;
  }

  if (!IsActorSuppressDialogsEnabled()) {
    return false;
  }

  switch (type) {
    case ActorSuppressedDialogType::kPermissions:
      return kActorSuppressDialogsPermissionsFeatureParam.Get();
  }
  NOTREACHED();
}

void RecordDialogSuppressedDuringActuation(IOSDialogDuringActuation dialog) {
  base::UmaHistogramEnumeration(kActorDialogSuppressedHistogram, dialog);
}
