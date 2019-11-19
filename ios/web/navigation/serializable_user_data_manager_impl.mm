// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/serializable_user_data_manager_impl.h"

#import "base/mac/foundation_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
// The key under which SerializableUserDataManagerWrapper are stored in the
// WebState's user data.
const void* const kSerializableUserDataManagerKey =
    &kSerializableUserDataManagerKey;
// The key under which SerializableUserDataImpl's data is encoded.
NSString* const kSerializedUserDataKey = @"serializedUserData";

// Wrapper class used to associate SerializableUserDataManagerImpls with its
// associated WebState.
class SerializableUserDataManagerWrapper : public base::SupportsUserData::Data {
 public:
  // Returns the SerializableUserDataManagerWrapper associated with |web_state|,
  // creating one if necessary.
  static SerializableUserDataManagerWrapper* FromWebState(
      web::WebState* web_state) {
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

  // Returns the manager owned by this wrapper.
  SerializableUserDataManagerImpl* manager() { return &manager_; }

 private:
  // The SerializableUserDataManagerWrapper owned by this object.
  SerializableUserDataManagerImpl manager_;
};
}  // namespace

// static
std::unique_ptr<SerializableUserData> SerializableUserData::Create() {
  return std::make_unique<SerializableUserDataImpl>();
}

SerializableUserDataImpl::SerializableUserDataImpl() : data_(@{}) {}

SerializableUserDataImpl::~SerializableUserDataImpl() {}

SerializableUserDataImpl::SerializableUserDataImpl(
    NSDictionary<NSString*, id<NSCoding>>* data)
    : data_([data copy]) {
  DCHECK(data_);
}

void SerializableUserDataImpl::Encode(NSCoder* coder) {
  [coder encodeObject:data_ forKey:kSerializedUserDataKey];
}

void SerializableUserDataImpl::Decode(NSCoder* coder) {
  data_ = [[coder decodeObjectForKey:kSerializedUserDataKey] mutableCopy];
  if (!data_) {
    // Ensure that there is always a dictionary even if there was no data
    // loaded from the coder (this can happen during unit testing or when
    // loading really old session).
    data_ = [NSMutableDictionary dictionary];
  }
  DCHECK(data_);
}

// static
SerializableUserDataManager* SerializableUserDataManager::FromWebState(
    web::WebState* web_state) {
  DCHECK(web_state);
  return SerializableUserDataManagerWrapper::FromWebState(web_state)->manager();
}

SerializableUserDataManagerImpl::SerializableUserDataManagerImpl()
    : data_([[NSMutableDictionary alloc] init]) {}

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

std::unique_ptr<SerializableUserData>
SerializableUserDataManagerImpl::CreateSerializableUserData() const {
  return std::make_unique<SerializableUserDataImpl>(data_);
}

void SerializableUserDataManagerImpl::AddSerializableUserData(
    SerializableUserData* data) {
  if (data) {
    SerializableUserDataImpl* data_impl =
        static_cast<SerializableUserDataImpl*>(data);
    data_ = [data_impl->data() mutableCopy];
    DCHECK(data_);
  }
}

}  // namespace web
