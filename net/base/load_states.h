// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_STATES_H__
#define NET_BASE_LOAD_STATES_H__

#include <string>


namespace net {

// These states correspond to the lengthy periods of time that a resource load
// may be blocked and unable to make progress.
enum LoadState {

#define LOAD_STATE(label, value) LOAD_STATE_##label,
#include "net/base/load_states_list.h"
#undef LOAD_STATE

};

// Some states, like LOAD_STATE_WAITING_FOR_DELEGATE, are associated with extra
// data that describes more precisely what the delegate (for example) is doing.
// This class provides an easy way to hold a load state with an extra parameter.
struct LoadStateWithParam {
  LoadState state;
  std::u16string param;
  LoadStateWithParam() : state(LOAD_STATE_IDLE) {}
  LoadStateWithParam(LoadState state, const std::u16string& param)
      : state(state), param(param) {}
};

}  // namespace net

#endif  // NET_BASE_LOAD_STATES_H__
