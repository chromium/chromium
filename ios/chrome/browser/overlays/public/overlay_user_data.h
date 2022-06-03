// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_USER_DATA_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_USER_DATA_H_

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"

// Macro for OverlayUserData setup [add to .h file]:
// - Declares a static variable inside subclasses.  The address of this static
//   variable is used as the key to associate the OverlayUserData with its
//   user data container.
// - Adds a friend specification for OverlayUserData so it can access
//   specializations' private constructors and user data keys.
#define OVERLAY_USER_DATA_SETUP(Type)    \
  static constexpr int kUserDataKey = 0; \
  friend class OverlayUserData<Type>

// Macro for OverlayUserData setup implementation [add to .cc/.mm file]:
// - Instantiates the static variable declared by the previous macro. It must
//   live in a .cc/.mm file to ensure that there is only one instantiation of
//   the static variable.
#define OVERLAY_USER_DATA_SETUP_IMPL(Type) const int Type::kUserDataKey

// A base class for classes attached to, and scoped to, the lifetime of a user
// data container (e.g. OverlayRequest, OverlayResponse).
//
// --- in data.h ---
// class Data : public OverlayUserData<Data> {
//  public:
//   ~Data() override;
//   // ... more public stuff here ...
//  private:
//   OVERLAY_USER_DATA_SETUP(Data);
//   explicit Data( \* ANY ARGUMENT LIST SUPPORTED *\);
//   // ... more private stuff here ...
// };
//
// --- in data.cc ---
// OVERLAY_USER_DATA_SETUP_IMPL(Data);
template <class DataType>
class OverlayUserData : public base::SupportsUserData::Data {
 public:
  // Creates an OverlayUserData of type DataType and adds it to |user_data|
  // under its key.  The DataType instance is constructed using the arguments
  // passed after the key to this function.  If a DataType instance already
  // exists in |user_data|, no new object is created.  For example, if the
  // constructor for an OverlayUserData of type StringData takes a string, one
  // can be created using:
  //
  // StringData::CreateForUserData(user_data, "string");
  template <typename... Args>
  static void CreateForUserData(base::SupportsUserData* user_data,
                                Args&&... args) {
    if (!FromUserData(user_data)) {
      std::unique_ptr<DataType> data =
          base::WrapUnique(new DataType(std::forward<Args>(args)...));
      data->CreateAuxiliaryData(user_data);
      user_data->SetUserData(UserDataKey(), std::move(data));
    }
  }

  // Retrieves the instance of type DataType that was attached to the specified
  // user data container and returns it. If no instance of the type was
  // attached, returns nullptr.
  static DataType* FromUserData(base::SupportsUserData* user_data) {
    return static_cast<DataType*>(user_data->GetUserData(UserDataKey()));
  }
  static const DataType* FromUserData(const base::SupportsUserData* user_data) {
    return static_cast<const DataType*>(user_data->GetUserData(UserDataKey()));
  }

  // The key under which to store the user data.
  static const void* UserDataKey() { return &DataType::kUserDataKey; }

 protected:
  // Adds auxilliary OverlayUserData to |data|.  Used to allow multiple
  // OverlayUserData templates to share common functionality in a separate data
  // stored in |user_data|.
  virtual void CreateAuxiliaryData(base::SupportsUserData* user_data) {}
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_USER_DATA_H_
