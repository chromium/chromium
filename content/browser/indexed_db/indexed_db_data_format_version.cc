// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_data_format_version.h"

#include "third_party/blink/public/web/web_serialized_script_value_version.h"
#include "v8/include/v8-value-serializer-version.h"

namespace content::indexed_db {

// static
IndexedDBDataFormatVersion IndexedDBDataFormatVersion::current_(
    v8::CurrentValueSerializerFormatVersion(),
    blink::kSerializedScriptValueVersion);

}  // namespace content::indexed_db
