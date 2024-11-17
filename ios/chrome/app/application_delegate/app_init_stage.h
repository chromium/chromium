// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_INIT_STAGE_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_INIT_STAGE_H_

// App initialization stages. The app will go sequentially in-order through each
// stage each time the app is launched. This enum might expand in the future but
// the start and last stages will always keep the same label and relative
// position.
enum class AppInitStage {
  // The first stage when starting the initialization. The label and value of
  // this enum item should not change.
  kStart = 0,

  // The app is starting the minimal basic browser services to support safe
  // mode.
  kBrowserBasic,

  // The app is considering whether safe mode should be used. The app will stay
  // at the kSafeMode stage if safe mode is needed, or will move to the next
  // stage otherwise.
  kSafeMode,

  // The app is waiting for the Finch seed to be fetched on first run. The app
  // will stay at the kVariationsSeed if it is the first launch after
  // installation, and the seed has not been fetched; it moves to the next stage
  // otherwise.
  kVariationsSeed,

  // The app is initializing the browser objects for the background handlers.
  // In particular this creates ChromeMain instances which initialises many
  // low-level objects (such as PostTask, ProfileManagerIOS, named threads,
  // ApplicationContext, ...). Using the corresponding features when the
  // AppInitStage is below this stage is unsupported. Most likely, you want all
  // new stages to be >= kBrowserObjectsForBackgroundHandlers.
  kBrowserObjectsForBackgroundHandlers,

  // The app is fetching any enterprise policies. The initialization is blocked
  // on this because the policies might have an effect on later init stages.
  kEnterprise,

  // TODO(crbug.com/353683675): All follow-up stage will eventually become
  // ProfileInitStage and will disappear. And corresponding code should move
  // from MainController to ProfileController.

  // TODO(crbug.com/333863468): code should no longer check this enum value,
  // instead it should use ProfileInitStage::kPrepareUI. The enum will be
  // removed once the AppInitStage and ProfileInitStage are fully decoupled.
  kLoadProfiles,

  // TODO(crbug.com/333863468): code should no longer check this enum value,
  // instead it should use ProfileInitStage::kPrepareUI. The enum will be
  // removed once the AppInitStage and ProfileInitStage are fully decoupled.
  kBrowserObjectsForUI,

  // TODO(crbug.com/333863468): code should no longer check this enum value,
  // instead it should use ProfileInitStage::kUIReady. The enum will be
  // removed once the AppInitStage and ProfileInitStage are fully decoupled.
  kNormalUI,

  // TODO(crbug.com/333863468): code should no longer check this enum value,
  // instead it should use ProfileInitStage::kFirstRun. The enum will be
  // removed once the AppInitStage and ProfileInitStage are fully decoupled.
  kFirstRun,

  // TODO(crbug.com/333863468): code should no longer check this enum value,
  // instead it should use ProfileInitStage::kChoiceScreen. The enum will be
  // removed once the AppInitStage and ProfileInitStage are fully decoupled.
  kChoiceScreen,

  // The final stage before being done with initialization. The label and
  // relative position (always last) of this enum item should not change.
  // The value may change when inserting enum items between Start and Final.
  kFinal,
};

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_INIT_STAGE_H_
