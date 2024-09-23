// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SHARED_DICTIONARY_SHARED_DICTIONARY_GETTER_H_
#define NET_SHARED_DICTIONARY_SHARED_DICTIONARY_GETTER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"

class GURL;

namespace net {

class SharedDictionary;

using SharedDictionaryGetter =
    base::RepeatingCallback<scoped_refptr<SharedDictionary>(
        const std::optional<SharedDictionaryIsolationKey>& isolation_key,
        const GURL& request_url)>;

}  // namespace net

#endif  // NET_SHARED_DICTIONARY_SHARED_DICTIONARY_GETTER_H_
