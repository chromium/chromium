// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_STATE_LIST_SERIALIZATION_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_STATE_LIST_SERIALIZATION_H_

#import <Foundation/Foundation.h>

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"

@class CRWSessionStorage;
@class SessionWindowIOS;
class WebStateList;

namespace web {
namespace proto {
class WebStateMetadataStorage;
}  // namespace proto
class WebState;
class WebStateID;
}  // namespace web

namespace ios::proto {
class WebStateListStorage;
}  // namespace ios::proto

// Factory for creating WebStates.
using WebStateFactory =
    base::RepeatingCallback<std::unique_ptr<web::WebState>(CRWSessionStorage*)>;

// Factory for creating WebStates from proto.
using WebStateFactoryFromProto = base::RepeatingCallback<std::unique_ptr<
    web::WebState>(web::WebStateID, web::proto::WebStateMetadataStorage)>;

// Mapping of WebStateID to their WebStateMetadataStorage.
using WebStateMetadataMap =
    std::map<web::WebStateID, web::proto::WebStateMetadataStorage>;

// Serializes `web_state_list` to a SessionWindowIOS instance.
SessionWindowIOS* SerializeWebStateList(const WebStateList* web_state_list);

// Serializes `web_state_list` metadata to `storage`.
void SerializeWebStateList(const WebStateList& web_state_list,
                           const WebStateMetadataMap& metadata_map,
                           ios::proto::WebStateListStorage& storage);

// Restores a `web_state_list` from `session_window` using `factory` to
// create the restored WebStates.
// If `enable_pinned_web_states` is false, the tabs are not marked as pinned
// upon restoration.
// If `enable_tab_groups` is false, tabs in groups are restored but groups are
// not.
// Returns a vector containing pointer to the restored WebStates. The
// pointers are still owned by the WebStateList, so they may become
// invalid as soon as the list is mutated.
//
// It is an error to call the method if `web_state_list` is not empty.
std::vector<web::WebState*> DeserializeWebStateList(
    WebStateList* web_state_list,
    SessionWindowIOS* session_window,
    bool enable_pinned_web_states,
    bool enable_tab_groups,
    const WebStateFactory& factory);

// Restores a `web_state_list` from `storage` using `factory` to create
// the restored WebStates.
// If `enable_pinned_web_states` is false, the tabs are not marked as pinned
// upon restoration.
// If `enable_tab_groups` is false, tabs in groups are restored but groups are
// not.
// Returns a vector containing pointer to the restored WebStates. The
// pointers are still owned by the WebStateList, so they may become
// invalid as soon as the list is mutated.
//
// It is an error to call the method if `web_state_list` is not empty.
std::vector<web::WebState*> DeserializeWebStateList(
    WebStateList* web_state_list,
    ios::proto::WebStateListStorage storage,
    bool enable_pinned_web_states,
    bool enable_tab_groups,
    const WebStateFactoryFromProto& factory);

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_STATE_LIST_SERIALIZATION_H_
