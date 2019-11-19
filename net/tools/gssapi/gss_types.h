// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_GSSAPI_GSS_TYPES_H_
#define NET_TOOLS_GSSAPI_GSS_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

// Define a minimal subset of the definitions needed to build a loadable fake
// GSSAPI library. The bindings follow RFC 2744. The code follows the RFC
// faithfully with the possible exception of `const` qualifiers for some
// function arguments.
//
// Note that //net/http/http_auth_gssapi_posix* functions depend on the gssapi.h
// as found on the host platform. For test purposes file does not depend on the
// system gssapi.h in order to reduce sensitivity to the host environment.
//
// These declarations follow RFC 2744 Appendix A with the exception of using
// C++isms in some places.

using OM_uint32 = uint32_t;
using gss_qop_t = uint32_t;

struct gss_buffer_desc_struct {
  size_t length;
  void* value;
};
using gss_buffer_desc = gss_buffer_desc_struct;
using gss_buffer_t = gss_buffer_desc_struct*;

struct gss_OID_desc_struct {
  OM_uint32 length;
  void* elements;
};
using gss_OID_desc = gss_OID_desc_struct;
using gss_OID = gss_OID_desc_struct*;

struct gss_channel_bindings_struct {
  OM_uint32 initiator_addrtype;
  gss_buffer_desc initiator_address;
  OM_uint32 acceptor_addrtype;
  gss_buffer_desc acceptor_address;
  gss_buffer_desc application_data;
};
using gss_channel_bindings_t = gss_channel_bindings_struct*;

// Following structures are defined as <implementation-specific>.

struct FakeGssName {};
using gss_name_t = FakeGssName*;

struct FakeGssCredId {};
using gss_cred_id_t = FakeGssCredId*;

struct FakeGssCtxId {};
using gss_ctx_id_t = FakeGssCtxId*;

#if defined(WIN32)
#define GSS_EXPORT __declspec(dllexport)
#else
#define GSS_EXPORT __attribute__((visibility("default")))
#endif

#endif  // NET_TOOLS_GSSAPI_GSS_TYPES_H_
