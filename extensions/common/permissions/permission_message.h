// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_H_

#include <string>
#include <vector>

#include "extensions/common/permissions/api_permission_set.h"

namespace extensions {

// The new kind of Chrome app/extension permission messages.
//
// A PermissionMessage is an immutable object that represents a single bullet
// in the list of an app or extension's permissions. It contains the localized
// permission message to display, as well as the set of permissions that
// contributed to that message (and should be revoked if this permission is
// revoked). It can also optionally contain a list of sub-messages which should
// appear as nested bullet points below the main one.
//
// |permissions| contains the permissions that are 'represented' by this
// message and should be revoked if this permission message is revoked. Note
// that other permissions could have contributed to the message, but these are
// the ones 'contained' in this message - if this set is taken for all
// PermissionMessages, each permission will only be in at most one
// PermissionMessage.
//
// Some permissions may contain nested messages, stored in |submessages|. These
// are appropriate to show as nested bullet points below the permission,
// collapsed if needed. For example, host permission messages may list all the
// sites the app has access to in |submessages|, with a summary message in
// |message|.
//
// TODO(sashab): Add a custom revoke action for each permission and nested
// permission message, registerable as a callback.
class PermissionMessage {
 public:
  PermissionMessage(const base::string16& message,
                    const PermissionIDSet& permissions);
  PermissionMessage(const base::string16& message,
                    const PermissionIDSet& permissions,
                    const std::vector<base::string16>& submessages);
  PermissionMessage(const PermissionMessage& other);
  virtual ~PermissionMessage();

  const base::string16& message() const { return message_; }
  const PermissionIDSet& permissions() const { return permissions_; }
  const std::vector<base::string16>& submessages() const {
    return submessages_;
  }

 private:
  const base::string16 message_;
  const PermissionIDSet permissions_;
  const std::vector<base::string16> submessages_;
};

using PermissionMessages = std::vector<PermissionMessage>;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_H_
