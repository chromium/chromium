// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/core_account_id.h"

CoreAccountId::CoreAccountId(const char* id) : id_(id) {}

CoreAccountId::CoreAccountId(std::string&& id) : id_(std::move(id)) {}

CoreAccountId::CoreAccountId(const std::string& id) : id_(id) {}
