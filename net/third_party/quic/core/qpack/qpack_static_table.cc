// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_static_table.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"

namespace quic {

// The "constructor" for a QpackStaticEntry that computes the lengths at
// compile time.
#define STATIC_ENTRY(name, value) \
  { name, QUIC_ARRAYSIZE(name) - 1, value, QUIC_ARRAYSIZE(value) - 1 }

const QpackStaticEntry kQpackStaticTable[] = {
    STATIC_ENTRY(":authority", ""),                                     // 0
    STATIC_ENTRY(":path", "/"),                                         // 1
    STATIC_ENTRY("age", "0"),                                           // 2
    STATIC_ENTRY("content-disposition", ""),                            // 3
    STATIC_ENTRY("content-length", "0"),                                // 4
    STATIC_ENTRY("cookie", ""),                                         // 5
    STATIC_ENTRY("date", ""),                                           // 6
    STATIC_ENTRY("etag", ""),                                           // 7
    STATIC_ENTRY("if-modified-since", ""),                              // 8
    STATIC_ENTRY("if-none-match", ""),                                  // 9
    STATIC_ENTRY("last-modified", ""),                                  // 10
    STATIC_ENTRY("link", ""),                                           // 11
    STATIC_ENTRY("location", ""),                                       // 12
    STATIC_ENTRY("referer", ""),                                        // 13
    STATIC_ENTRY("set-cookie", ""),                                     // 14
    STATIC_ENTRY(":method", "CONNECT"),                                 // 15
    STATIC_ENTRY(":method", "DELETE"),                                  // 16
    STATIC_ENTRY(":method", "GET"),                                     // 17
    STATIC_ENTRY(":method", "HEAD"),                                    // 18
    STATIC_ENTRY(":method", "OPTIONS"),                                 // 19
    STATIC_ENTRY(":method", "POST"),                                    // 20
    STATIC_ENTRY(":method", "PUT"),                                     // 21
    STATIC_ENTRY(":scheme", "http"),                                    // 22
    STATIC_ENTRY(":scheme", "https"),                                   // 23
    STATIC_ENTRY(":status", "103"),                                     // 24
    STATIC_ENTRY(":status", "200"),                                     // 25
    STATIC_ENTRY(":status", "304"),                                     // 26
    STATIC_ENTRY(":status", "404"),                                     // 27
    STATIC_ENTRY(":status", "503"),                                     // 28
    STATIC_ENTRY("accept", "*/*"),                                      // 29
    STATIC_ENTRY("accept", "application/dns-message"),                  // 30
    STATIC_ENTRY("accept-encoding", "gzip, deflate, br"),               // 31
    STATIC_ENTRY("accept-ranges", "bytes"),                             // 32
    STATIC_ENTRY("access-control-allow-headers", "cache-control"),      // 33
    STATIC_ENTRY("access-control-allow-headers", "content-type"),       // 35
    STATIC_ENTRY("access-control-allow-origin", "*"),                   // 35
    STATIC_ENTRY("cache-control", "max-age=0"),                         // 36
    STATIC_ENTRY("cache-control", "max-age=2592000"),                   // 37
    STATIC_ENTRY("cache-control", "max-age=604800"),                    // 38
    STATIC_ENTRY("cache-control", "no-cache"),                          // 39
    STATIC_ENTRY("cache-control", "no-store"),                          // 40
    STATIC_ENTRY("cache-control", "public, max-age=31536000"),          // 41
    STATIC_ENTRY("content-encoding", "br"),                             // 42
    STATIC_ENTRY("content-encoding", "gzip"),                           // 43
    STATIC_ENTRY("content-type", "application/dns-message"),            // 44
    STATIC_ENTRY("content-type", "application/javascript"),             // 45
    STATIC_ENTRY("content-type", "application/json"),                   // 46
    STATIC_ENTRY("content-type", "application/x-www-form-urlencoded"),  // 47
    STATIC_ENTRY("content-type", "image/gif"),                          // 48
    STATIC_ENTRY("content-type", "image/jpeg"),                         // 49
    STATIC_ENTRY("content-type", "image/png"),                          // 50
    STATIC_ENTRY("content-type", "text/css"),                           // 51
    STATIC_ENTRY("content-type", "text/html; charset=utf-8"),           // 52
    STATIC_ENTRY("content-type", "text/plain"),                         // 53
    STATIC_ENTRY("content-type", "text/plain;charset=utf-8"),           // 54
    STATIC_ENTRY("range", "bytes=0-"),                                  // 55
    STATIC_ENTRY("strict-transport-security", "max-age=31536000"),      // 56
    STATIC_ENTRY("strict-transport-security",
                 "max-age=31536000; includesubdomains"),  // 57
    STATIC_ENTRY("strict-transport-security",
                 "max-age=31536000; includesubdomains; preload"),        // 58
    STATIC_ENTRY("vary", "accept-encoding"),                             // 59
    STATIC_ENTRY("vary", "origin"),                                      // 60
    STATIC_ENTRY("x-content-type-options", "nosniff"),                   // 61
    STATIC_ENTRY("x-xss-protection", "1; mode=block"),                   // 62
    STATIC_ENTRY(":status", "100"),                                      // 63
    STATIC_ENTRY(":status", "204"),                                      // 64
    STATIC_ENTRY(":status", "206"),                                      // 65
    STATIC_ENTRY(":status", "302"),                                      // 66
    STATIC_ENTRY(":status", "400"),                                      // 67
    STATIC_ENTRY(":status", "403"),                                      // 68
    STATIC_ENTRY(":status", "421"),                                      // 69
    STATIC_ENTRY(":status", "425"),                                      // 70
    STATIC_ENTRY(":status", "500"),                                      // 71
    STATIC_ENTRY("accept-language", ""),                                 // 72
    STATIC_ENTRY("access-control-allow-credentials", "FALSE"),           // 73
    STATIC_ENTRY("access-control-allow-credentials", "TRUE"),            // 74
    STATIC_ENTRY("access-control-allow-headers", "*"),                   // 75
    STATIC_ENTRY("access-control-allow-methods", "get"),                 // 76
    STATIC_ENTRY("access-control-allow-methods", "get, post, options"),  // 77
    STATIC_ENTRY("access-control-allow-methods", "options"),             // 78
    STATIC_ENTRY("access-control-expose-headers", "content-length"),     // 79
    STATIC_ENTRY("access-control-request-headers", "content-type"),      // 80
    STATIC_ENTRY("access-control-request-method", "get"),                // 81
    STATIC_ENTRY("access-control-request-method", "post"),               // 82
    STATIC_ENTRY("alt-svc", "clear"),                                    // 83
    STATIC_ENTRY("authorization", ""),                                   // 84
    STATIC_ENTRY(
        "content-security-policy",
        "script-src 'none'; object-src 'none'; base-uri 'none'"),  // 85
    STATIC_ENTRY("early-data", "1"),                               // 86
    STATIC_ENTRY("expect-ct", ""),                                 // 87
    STATIC_ENTRY("forwarded", ""),                                 // 88
    STATIC_ENTRY("if-range", ""),                                  // 89
    STATIC_ENTRY("origin", ""),                                    // 90
    STATIC_ENTRY("purpose", "prefetch"),                           // 91
    STATIC_ENTRY("server", ""),                                    // 92
    STATIC_ENTRY("timing-allow-origin", "*"),                      // 93
    STATIC_ENTRY("upgrade-insecure-requests", "1"),                // 94
    STATIC_ENTRY("user-agent", ""),                                // 95
    STATIC_ENTRY("x-forwarded-for", ""),                           // 96
    STATIC_ENTRY("x-frame-options", "deny"),                       // 97
    STATIC_ENTRY("x-frame-options", "sameorigin"),                 // 98
};

#undef STATIC_ENTRY

namespace {

QpackStaticTable* InitializeSharedStaticTable() {
  auto shared_static_table = new QpackStaticTable();
  shared_static_table->Initialize(kQpackStaticTable,
                                  QUIC_ARRAYSIZE(kQpackStaticTable));
  CHECK(shared_static_table->IsInitialized());
  return shared_static_table;
}

}  // namespace

const QpackStaticTable& ObtainQpackStaticTable() {
  static QpackStaticTable* shared_static_table = InitializeSharedStaticTable();
  return *shared_static_table;
}

}  // namespace quic
