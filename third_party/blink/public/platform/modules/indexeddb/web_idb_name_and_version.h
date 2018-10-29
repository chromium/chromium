#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_NAME_AND_VERSION_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_NAME_AND_VERSION_H_

#include "third_party/blink/public/platform/web_string.h"

namespace blink {

struct WebIDBNameAndVersion {
  enum { kNoVersion = -1 };
  WebString name;
  int64_t version;

  WebIDBNameAndVersion() : version(kNoVersion) {}
  WebIDBNameAndVersion(WebString name, int64_t version)
      : name(name), version(version) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_NAME_AND_VERSION_H_
