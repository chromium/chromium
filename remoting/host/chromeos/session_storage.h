// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_SESSION_STORAGE_H_
#define REMOTING_HOST_CHROMEOS_SESSION_STORAGE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/values.h"

namespace remoting {

// Utilities to store CRD session information, which allows us to later resume
// the session.
class SessionStorage {
 public:
  virtual ~SessionStorage() = default;

  virtual void StoreSession(const base::Value::Dict& information,
                            base::OnceClosure on_done) = 0;
  virtual void DeleteSession(base::OnceClosure on_done) = 0;
  virtual void RetrieveSession(
      base::OnceCallback<void(std::optional<base::Value::Dict>)> on_done) = 0;

  virtual void HasSession(base::OnceCallback<void(bool)> on_done) const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_SESSION_STORAGE_H_
