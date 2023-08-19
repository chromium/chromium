// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#define CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_

#include "content/common/content_export.h"
#include "content/public/common/webplugininfo_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "services/network/public/cpp/network_ipc_param_traits.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#if BUILDFLAG(IS_MAC)
#include "content/public/common/drop_data.h"
#include "content/public/common/referrer.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_VALIDATE(ui::PageTransition,
                         ((value &
                           ui::PageTransition::PAGE_TRANSITION_CORE_MASK) <=
                          ui::PageTransition::PAGE_TRANSITION_LAST_CORE))

#if BUILDFLAG(IS_MAC)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::ReferrerPolicy,
                          network::mojom::ReferrerPolicy::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(content::DropData::Kind,
                          content::DropData::Kind::LAST)

IPC_STRUCT_TRAITS_BEGIN(ui::FileInfo)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DropData)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(url_title)
  IPC_STRUCT_TRAITS_MEMBER(download_metadata)
  IPC_STRUCT_TRAITS_MEMBER(referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(filenames)
  IPC_STRUCT_TRAITS_MEMBER(filesystem_id)
  IPC_STRUCT_TRAITS_MEMBER(file_system_files)
  IPC_STRUCT_TRAITS_MEMBER(text)
  IPC_STRUCT_TRAITS_MEMBER(html)
  IPC_STRUCT_TRAITS_MEMBER(html_base_url)
  IPC_STRUCT_TRAITS_MEMBER(file_contents)
  IPC_STRUCT_TRAITS_MEMBER(file_contents_source_url)
  IPC_STRUCT_TRAITS_MEMBER(file_contents_filename_extension)
  IPC_STRUCT_TRAITS_MEMBER(file_contents_content_disposition)
  IPC_STRUCT_TRAITS_MEMBER(custom_data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DropData::FileSystemFileInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(filesystem_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DropData::Metadata)
  IPC_STRUCT_TRAITS_MEMBER(kind)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(filename)
  IPC_STRUCT_TRAITS_MEMBER(file_system_url)
IPC_STRUCT_TRAITS_END()

#endif  // BUILDFLAG(IS_MAC)

#endif  // CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
