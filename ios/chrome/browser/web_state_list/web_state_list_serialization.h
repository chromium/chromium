// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/callback_forward.h"

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
// If `web_states_to_serialize` is nil, all web states in `web_state_list` are
// serialized and their data is returned in SessionWindowIOS.tabContents.
// If `web_states_to_serialize` is not nil, it can contain the ID of the
// WebStates for which the content should be serialized. If a WebState ID is not
// in web_states_to_serialize, the result SessionWindowIOS.tabContents will not
// contain its data.
// All webStates are included in SessionWindowIOS.sessions and
// SessionWindowIOS.sessionSummary.
// `web_states_to_serialize` is ignored if kSaveSessionTabsToSeparateFiles is
// disabled.
// Until legacy session saving is disabled, setting `web_states_to_serialize`
// will not provide any performance improvement as legacy session saving
// serializes every webStates.
SessionWindowIOS* SerializeWebStateList(WebStateList* web_state_list,
                                        NSSet* web_states_to_serialize);

// Returns an array of serialised sessions.
SessionWindowIOS* SerializeWebStateList(WebStateList* web_state_list);

// Restores a `web_state_list` from `session_window` using `web_state_factory`
// to create the restored WebStates.
void DeserializeWebStateList(WebStateList* web_state_list,
                             SessionWindowIOS* session_window,
                             const WebStateFactory& web_state_factory);

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_
