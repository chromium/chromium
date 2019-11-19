// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_IPC_TAGS_H__
#define SANDBOX_SRC_IPC_TAGS_H__

namespace sandbox {

enum class IpcTag {
  UNUSED = 0,
  PING1,  // Takes a cookie in parameters and returns the cookie
          // multiplied by 2 and the tick_count. Used for testing only.
  PING2,  // Takes an in/out cookie in parameters and modify the cookie
          // to be multiplied by 3. Used for testing only.
  NTCREATEFILE,
  NTOPENFILE,
  NTQUERYATTRIBUTESFILE,
  NTQUERYFULLATTRIBUTESFILE,
  NTSETINFO_RENAME,
  CREATENAMEDPIPEW,
  NTOPENTHREAD,
  NTOPENPROCESS,
  NTOPENPROCESSTOKEN,
  NTOPENPROCESSTOKENEX,
  CREATEPROCESSW,
  CREATEEVENT,
  OPENEVENT,
  NTCREATEKEY,
  NTOPENKEY,
  GDI_GDIDLLINITIALIZE,
  GDI_GETSTOCKOBJECT,
  USER_REGISTERCLASSW,
  CREATETHREAD,
  USER_ENUMDISPLAYMONITORS,
  USER_ENUMDISPLAYDEVICES,
  USER_GETMONITORINFO,
  GDI_CREATEOPMPROTECTEDOUTPUTS,
  GDI_GETCERTIFICATE,
  GDI_GETCERTIFICATESIZE,
  GDI_DESTROYOPMPROTECTEDOUTPUT,
  GDI_CONFIGUREOPMPROTECTEDOUTPUT,
  GDI_GETOPMINFORMATION,
  GDI_GETOPMRANDOMNUMBER,
  GDI_GETSUGGESTEDOPMPROTECTEDOUTPUTARRAYSIZE,
  GDI_SETOPMSIGNINGKEYANDSEQUENCENUMBERS,
  NTCREATESECTION,
  LAST
};

constexpr size_t kMaxServiceCount = 64;
constexpr size_t kMaxIpcTag = static_cast<size_t>(IpcTag::LAST);
static_assert(kMaxIpcTag <= kMaxServiceCount, "kMaxServiceCount is too low");

}  // namespace sandbox

#endif  // SANDBOX_SRC_IPC_TAGS_H__
