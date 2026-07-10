// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_PASSKEY_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_PASSKEY_H_

#include "base/types/pass_key.h"

namespace layout_state {

// Forward-declare the C++ helper classes that are defined exclusively inside
// their respective authorized implementation (.mm) files.
class SceneViewControllerPassKeyFactory;
class SceneCoordinatorPassKeyFactory;
class AssistantContainerViewControllerPassKeyFactory;
class AssistantContainerAnimatorPassKeyFactory;
class MainToolbarCoordinatorPassKeyFactory;
class MainToolbarMediatorPassKeyFactory;
class AppBarMediatorPassKeyFactory;

// Forward-declare the test helper class defined in
// layout_state_test_passkey_factory.h.
class LayoutStateTestPassKeyFactory;

}  // namespace layout_state

// Global typedef aliases for Objective-C++ method declaration compatibility.
// These use Chromium's variadic base::PassKey template to authorize multiple
// domain-specific helper classes and the test helper class.

typedef base::PassKey<layout_state::SceneViewControllerPassKeyFactory,
                      layout_state::SceneCoordinatorPassKeyFactory,
                      layout_state::LayoutStateTestPassKeyFactory>
    LayoutStateScenePassKey;

typedef base::PassKey<
    layout_state::AssistantContainerViewControllerPassKeyFactory,
    layout_state::AssistantContainerAnimatorPassKeyFactory,
    layout_state::AppBarMediatorPassKeyFactory,
    layout_state::LayoutStateTestPassKeyFactory>
    LayoutStateAssistantPassKey;

typedef base::PassKey<layout_state::MainToolbarCoordinatorPassKeyFactory,
                      layout_state::MainToolbarMediatorPassKeyFactory,
                      layout_state::LayoutStateTestPassKeyFactory>
    LayoutStateToolbarPassKey;

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LAYOUT_STATE_PASSKEY_H_
