// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_TEST_PASSKEY_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_TEST_PASSKEY_FACTORY_H_

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_passkey.h"

namespace layout_state {

// C++ companion class for testing, authorized to generate all layout passkeys.
class LayoutStateTestPassKeyFactory {
 public:
  static base::PassKey<LayoutStateTestPassKeyFactory> CreateSceneKey() {
    return base::PassKey<LayoutStateTestPassKeyFactory>();
  }
  static base::PassKey<LayoutStateTestPassKeyFactory> CreateAssistantKey() {
    return base::PassKey<LayoutStateTestPassKeyFactory>();
  }
  static base::PassKey<LayoutStateTestPassKeyFactory> CreateToolbarKey() {
    return base::PassKey<LayoutStateTestPassKeyFactory>();
  }
  static base::PassKey<LayoutStateTestPassKeyFactory> CreateBrowserKey() {
    return base::PassKey<LayoutStateTestPassKeyFactory>();
  }
};

}  // namespace layout_state

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_TEST_PASSKEY_FACTORY_H_
