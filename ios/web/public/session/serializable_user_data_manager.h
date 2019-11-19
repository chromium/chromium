// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_SERIALIZABLE_USER_DATA_MANAGER_H_
#define IOS_WEB_PUBLIC_SESSION_SERIALIZABLE_USER_DATA_MANAGER_H_

#import <Foundation/Foundation.h>
#include <memory>

#include "base/macros.h"

namespace web {

class WebState;

// Class used to serialize values added to SerializableUserDataManager.
class SerializableUserData {
 public:
  virtual ~SerializableUserData() = default;

  // Factory method.
  static std::unique_ptr<SerializableUserData> Create();

  // Encodes the data with |coder|.
  virtual void Encode(NSCoder* coder) = 0;

  // Decodes the data from |coder|.
  virtual void Decode(NSCoder* coder) = 0;

 protected:
  SerializableUserData() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerializableUserData);
};

// Class that can be used to add serializable user data to a WebState.
class SerializableUserDataManager {
 public:
  // Returns the SerializableUserDataManager instance associated with
  // |web_state|, instantiating one if necessary.
  static SerializableUserDataManager* FromWebState(web::WebState* web_state);

  // Adds |data| to the user data, allowing it to be encoded under |key|.
  // |data| is expected to be non-nil.  If |key| has already been used, its
  // associated value will be overwritten.
  virtual void AddSerializableData(id<NSCoding> data, NSString* key) = 0;

  // Returns the value that has been stored under |key|.
  virtual id<NSCoding> GetValueForSerializationKey(NSString* key) = 0;

  // Creates a SerializableUserData that can be used to encode the values added
  // to the manager.
  virtual std::unique_ptr<SerializableUserData> CreateSerializableUserData()
      const = 0;

  // Adds the values decoded from |data| to the manager.
  virtual void AddSerializableUserData(SerializableUserData* data) = 0;

 protected:
  SerializableUserDataManager() = default;
  ~SerializableUserDataManager() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerializableUserDataManager);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SESSION_SERIALIZABLE_USER_DATA_MANAGER_H_
