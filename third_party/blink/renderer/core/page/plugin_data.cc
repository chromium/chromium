/*
    Copyright (C) 2000 Harri Porten (porten@kde.org)
    Copyright (C) 2000 Daniel Molkentin (molkentin@kde.org)
    Copyright (C) 2000 Stefan Schimanski (schimmi@kde.org)
    Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All Rights Reserved.
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/page/plugin_data.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

void MimeClassInfo::Trace(blink::Visitor* visitor) {
  visitor->Trace(plugin_);
}

MimeClassInfo::MimeClassInfo(const String& type,
                             const String& description,
                             PluginInfo& plugin)
    : type_(type), description_(description), plugin_(&plugin) {}

void PluginInfo::Trace(blink::Visitor* visitor) {
  visitor->Trace(mimes_);
}

PluginInfo::PluginInfo(const String& name,
                       const String& filename,
                       const String& description,
                       Color background_color,
                       bool may_use_external_handler)
    : name_(name),
      filename_(filename),
      description_(description),
      background_color_(background_color),
      may_use_external_handler_(may_use_external_handler) {}

void PluginInfo::AddMimeType(MimeClassInfo* info) {
  mimes_.push_back(info);
}

const MimeClassInfo* PluginInfo::GetMimeClassInfo(wtf_size_t index) const {
  if (index >= mimes_.size())
    return nullptr;
  return mimes_[index];
}

const MimeClassInfo* PluginInfo::GetMimeClassInfo(const String& type) const {
  for (MimeClassInfo* mime : mimes_) {
    if (mime->Type() == type)
      return mime;
  }

  return nullptr;
}

wtf_size_t PluginInfo::GetMimeClassInfoSize() const {
  return mimes_.size();
}

void PluginData::Trace(blink::Visitor* visitor) {
  visitor->Trace(plugins_);
  visitor->Trace(mimes_);
}

// static
void PluginData::RefreshBrowserSidePluginCache() {
  mojo::Remote<mojom::blink::PluginRegistry> registry;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      registry.BindNewPipeAndPassReceiver());
  Vector<mojom::blink::PluginInfoPtr> plugins;
  registry->GetPlugins(true, SecurityOrigin::CreateUniqueOpaque(), &plugins);
}

void PluginData::UpdatePluginList(const SecurityOrigin* main_frame_origin) {
  ResetPluginData();
  main_frame_origin_ = main_frame_origin;

  mojo::Remote<mojom::blink::PluginRegistry> registry;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      registry.BindNewPipeAndPassReceiver());
  Vector<mojom::blink::PluginInfoPtr> plugins;
  registry->GetPlugins(false, main_frame_origin_, &plugins);
  for (const auto& plugin : plugins) {
    auto* plugin_info = MakeGarbageCollected<PluginInfo>(
        plugin->name, FilePathToWebString(plugin->filename),
        plugin->description, plugin->background_color,
        plugin->may_use_external_handler);
    plugins_.push_back(plugin_info);
    for (const auto& mime : plugin->mime_types) {
      auto* mime_info = MakeGarbageCollected<MimeClassInfo>(
          mime->mime_type, mime->description, *plugin_info);
      mime_info->extensions_ = mime->file_extensions;
      plugin_info->AddMimeType(mime_info);
      mimes_.push_back(mime_info);
    }
  }

  std::sort(
      plugins_.begin(), plugins_.end(),
      [](const Member<PluginInfo>& lhs, const Member<PluginInfo>& rhs) -> bool {
        return WTF::CodeUnitCompareLessThan(lhs->Name(), rhs->Name());
      });
  std::sort(mimes_.begin(), mimes_.end(),
            [](const Member<MimeClassInfo>& lhs,
               const Member<MimeClassInfo>& rhs) -> bool {
              return WTF::CodeUnitCompareLessThan(lhs->Type(), rhs->Type());
            });
}

void PluginData::ResetPluginData() {
  plugins_.clear();
  mimes_.clear();
  main_frame_origin_ = nullptr;
}

bool PluginData::SupportsMimeType(const String& mime_type) const {
  for (const MimeClassInfo* info : mimes_) {
    if (info->type_ == mime_type)
      return true;
  }

  return false;
}

Color PluginData::PluginBackgroundColorForMimeType(
    const String& mime_type) const {
  for (const MimeClassInfo* info : mimes_) {
    if (info->type_ == mime_type)
      return info->Plugin()->BackgroundColor();
  }
  NOTREACHED();
  return Color();
}

bool PluginData::IsExternalPluginMimeType(const String& mime_type) const {
  for (const MimeClassInfo* info : mimes_) {
    if (info->type_ == mime_type)
      return info->Plugin()->MayUseExternalHandler();
  }
  return false;
}

}  // namespace blink
