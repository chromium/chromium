// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/serializable_user_data_manager_impl.h"

#import "base/apple/foundation_util.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/web_state.h"

namespace web {
namespace {
// The key under which SerializableUserDataManagerWrapper are stored in the
// WebState's user data.
const void* const kSerializableUserDataManagerKey =
    &kSerializableUserDataManagerKey;

// Wrapper class used to associate SerializableUserDataManagerImpls with its
// associated WebState.
class SerializableUserDataManagerWrapper : public base::SupportsUserData::Data {
 public:
  // Returns the SerializableUserDataManagerWrapper associated with `web_state`,
  // creating one if necessary.
  static SerializableUserDataManagerWrapper* FromWebState(WebState* web_state) {
    DCHECK(web_state);
    SerializableUserDataManagerWrapper* wrapper =
        static_cast<SerializableUserDataManagerWrapper*>(
            web_state->GetUserData(kSerializableUserDataManagerKey));
    if (wrapper)
      return wrapper;

    web_state->SetUserData(
        kSerializableUserDataManagerKey,
        std::make_unique<SerializableUserDataManagerWrapper>());
    return static_cast<SerializableUserDataManagerWrapper*>(
        web_state->GetUserData(kSerializableUserDataManagerKey));
  }

  static const SerializableUserDataManagerWrapper* FromWebState(
      const WebState* web_state) {
    DCHECK(web_state);
    const SerializableUserDataManagerWrapper* wrapper =
        static_cast<const SerializableUserDataManagerWrapper*>(
            web_state->GetUserData(kSerializableUserDataManagerKey));
    return wrapper;
  }

  // Returns the manager owned by this wrapper.
  SerializableUserDataManagerImpl* manager() { return &manager_; }
  const SerializableUserDataManagerImpl* manager() const { return &manager_; }

 private:
  // The SerializableUserDataManagerWrapper owned by this object.
  SerializableUserDataManagerImpl manager_;
};
}  // namespace

// static
SerializableUserDataManager* SerializableUserDataManager::FromWebState(
    WebState* web_state) {
  DCHECK(web_state);
  return SerializableUserDataManagerWrapper::FromWebState(web_state)->manager();
}

// static
const SerializableUserDataManager* SerializableUserDataManager::FromWebState(
    const WebState* web_state) {
  DCHECK(web_state);
  const SerializableUserDataManagerWrapper* wrapper =
      SerializableUserDataManagerWrapper::FromWebState(web_state);
  return wrapper ? wrapper->manager() : nullptr;
}

SerializableUserDataManagerImpl::SerializableUserDataManagerImpl()
    : data_([[CRWSessionUserData alloc] init]) {}

SerializableUserDataManagerImpl::~SerializableUserDataManagerImpl() {}

void SerializableUserDataManagerImpl::AddSerializableData(id<NSCoding> data,
                                                          NSString* key) {
  DCHECK(data);
  DCHECK(key.length);
  [data_ setObject:data forKey:key];
}

id<NSCoding> SerializableUserDataManagerImpl::GetValueForSerializationKey(
    NSString* key) {
  return [data_ objectForKey:key];
}

CRWSessionUserData* SerializableUserDataManagerImpl::GetUserDataForSession()
    const {
  return data_;
}

void SerializableUserDataManagerImpl::SetUserDataFromSession(
    CRWSessionUserData* data) {
  if (data) {
    data_ = data;
  } else {
    data = [[CRWSessionUserData alloc] init];
  }
}

}  // namespace web
