// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_SERIALIZABLE_USER_DATA_MANAGER_H_
#define IOS_WEB_PUBLIC_SESSION_SERIALIZABLE_USER_DATA_MANAGER_H_

#import <Foundation/Foundation.h>

@class CRWSessionUserData;

namespace web {

class WebState;

// Class that can be used to add serializable user data to a WebState.
class SerializableUserDataManager {
 public:
  // Returns the SerializableUserDataManager instance associated with
  // `web_state`, instantiating one if necessary (only for non-const
  // version).
  static SerializableUserDataManager* FromWebState(WebState* web_state);
  static const SerializableUserDataManager* FromWebState(
      const WebState* web_state);

  SerializableUserDataManager(const SerializableUserDataManager&) = delete;
  SerializableUserDataManager& operator=(const SerializableUserDataManager&) =
      delete;

  // Adds `data` to the user data, allowing it to be encoded under `key`.
  // `data` is expected to be non-nil.  If `key` has already been used, its
  // associated value will be overwritten.
  virtual void AddSerializableData(id<NSCoding> data, NSString* key) = 0;

  // Returns the value that has been stored under `key`.
  virtual id<NSCoding> GetValueForSerializationKey(NSString* key) = 0;

  // Returns a representation of the user data that can be serialized as
  // part of the session serialization.
  virtual CRWSessionUserData* GetUserDataForSession() const = 0;

  // Sets the user data the serialized object read from the session.
  virtual void SetUserDataFromSession(CRWSessionUserData* data) = 0;

 protected:
  SerializableUserDataManager() = default;
  ~SerializableUserDataManager() = default;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SESSION_SERIALIZABLE_USER_DATA_MANAGER_H_
