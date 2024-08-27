// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/application_state_mojom_traits.h"

namespace mojo {

// static
mojo_base::mojom::ApplicationState EnumTraits<
    mojo_base::mojom::ApplicationState,
    base::android::ApplicationState>::ToMojom(base::android::ApplicationState
                                                  input) {
  switch (input) {
    case base::android::APPLICATION_STATE_UNKNOWN:
      return mojo_base::mojom::ApplicationState::UNKNOWN;
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      return mojo_base::mojom::ApplicationState::HAS_RUNNING_ACTIVITIES;
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
      return mojo_base::mojom::ApplicationState::HAS_PAUSED_ACTIVITIES;
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
      return mojo_base::mojom::ApplicationState::HAS_STOPPED_ACTIVITIES;
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return mojo_base::mojom::ApplicationState::HAS_DESTROYED_ACTIVITIES;
  }
  NOTREACHED();
}

// static
bool EnumTraits<mojo_base::mojom::ApplicationState,
                base::android::ApplicationState>::
    FromMojom(mojo_base::mojom::ApplicationState input,
              base::android::ApplicationState* output) {
  switch (input) {
    case mojo_base::mojom::ApplicationState::UNKNOWN:
      *output = base::android::ApplicationState::APPLICATION_STATE_UNKNOWN;
      return true;
    case mojo_base::mojom::ApplicationState::HAS_RUNNING_ACTIVITIES:
      *output = base::android::ApplicationState::
          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
      return true;
    case mojo_base::mojom::ApplicationState::HAS_PAUSED_ACTIVITIES:
      *output = base::android::ApplicationState::
          APPLICATION_STATE_HAS_PAUSED_ACTIVITIES;
      return true;
    case mojo_base::mojom::ApplicationState::HAS_STOPPED_ACTIVITIES:
      *output = base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES;
      return true;
    case mojo_base::mojom::ApplicationState::HAS_DESTROYED_ACTIVITIES:
      *output = base::android::ApplicationState::
          APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES;
      return true;
  }
  return false;
}

}  // namespace mojo
