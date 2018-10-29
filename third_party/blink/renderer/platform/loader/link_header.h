// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_LINK_HEADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_LINK_HEADER_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LinkHeader {
  DISALLOW_NEW();

 public:
  const String& Url() const { return url_; }
  const String& Rel() const { return rel_; }
  const String& As() const { return as_; }
  const String& MimeType() const { return mime_type_; }
  const String& Media() const { return media_; }
  const String& CrossOrigin() const { return cross_origin_; }
  const String& Nonce() const { return nonce_; }
  const String& Integrity() const { return integrity_; }
  const String& Srcset() const { return srcset_; }
  const String& Imgsizes() const { return imgsizes_; }
  bool Valid() const { return is_valid_; }

  enum LinkParameterName {
    kLinkParameterRel,
    kLinkParameterAnchor,
    kLinkParameterTitle,
    kLinkParameterMedia,
    kLinkParameterType,
    kLinkParameterRev,
    kLinkParameterHreflang,
    // Beyond this point, only link-extension parameters
    kLinkParameterUnknown,
    kLinkParameterCrossOrigin,
    kLinkParameterAs,
    kLinkParameterNonce,
    kLinkParameterIntegrity,
    kLinkParameterSrcset,
    kLinkParameterImgsizes,
  };

 private:
  friend class LinkHeaderSet;

  template <typename Iterator>
  LinkHeader(Iterator begin, Iterator end);
  void SetValue(LinkParameterName, const String& value);

  String url_;
  String rel_;
  String as_;
  String mime_type_;
  String media_;
  String cross_origin_;
  String nonce_;
  String integrity_;
  String srcset_;
  String imgsizes_;
  bool is_valid_;
};

class PLATFORM_EXPORT LinkHeaderSet {
  STACK_ALLOCATED();

 public:
  LinkHeaderSet(const String& header);

  Vector<LinkHeader>::const_iterator begin() const {
    return header_set_.begin();
  }
  Vector<LinkHeader>::const_iterator end() const { return header_set_.end(); }
  LinkHeader& operator[](size_t i) { return header_set_[i]; }
  size_t size() { return header_set_.size(); }

 private:
  Vector<LinkHeader> header_set_;
};

}  // namespace blink

#endif
