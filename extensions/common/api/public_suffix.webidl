// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The encoding to use for returned domains.
enum DomainEncoding {
  "punycode",
  "display"
};

dictionary DomainOptions {
  // The encoding to use for the returned domain. Defaults to "punycode", but
  // pass "display" if displaying the domain to the user. Generally, "display"
  // will use Unicode, except in cases where that would be unsafe due to
  // <a href="https://www.unicode.org/reports/tr39/#Confusable_Detection">
  // "Unicode confusables"</a>.
  DomainEncoding encoding;

  // Whether IP addresses may be returned as domains. Defaults to false.
  // Note: When allowed, IP addresses are returned without being looked up in
  // the Public Suffix List.
  boolean allowIPAddress;

  // Whether known public suffixes may be returned as domains. Defaults to
  // false.
  boolean allowPlainSuffix;

  // Whether unknown TLDs should be considered as a public suffix. For example,
  // for non-public hostnames like <code>printer.internal-network</code>.
  // Defaults to false.
  boolean allowUnknownSuffix;
};

// Use the <code>chrome.publicSuffix</code> API to query the browser's Public
// Suffix List (PSL).
[nodoc] interface PublicSuffix {
  // Determines whether |hostname| is itself a known public suffix.
  [nocompile] static boolean isKnownSuffix(DOMString hostname);

  // Gets the known public suffix for |hostname|, if any.
  [nocompile] static DOMString? getKnownSuffix(DOMString hostname);

  // Gets the registrable domain for |hostname|, if any.
  [nocompile] static DOMString? getDomain(
      DOMString hostname,
      optional DomainOptions options);
};

partial interface Browser {
  static attribute PublicSuffix publicSuffix;
};
