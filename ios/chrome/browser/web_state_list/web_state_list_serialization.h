// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/functional/callback_forward.h"

@class CRWSessionStorage;
@class SessionWindowIOS;
class WebStateList;

namespace web {
class WebState;
}

// Factory for creating WebStates.
using WebStateFactory =
    base::RepeatingCallback<std::unique_ptr<web::WebState>(CRWSessionStorage*)>;

// Returns an array of serialised sessions.
SessionWindowIOS* SerializeWebStateList(WebStateList* web_state_list);

// Restores a `web_state_list` from `session_window` using `web_state_factory`
// to create the restored WebStates.
void DeserializeWebStateList(WebStateList* web_state_list,
                             SessionWindowIOS* session_window,
                             const WebStateFactory& web_state_factory);

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_
