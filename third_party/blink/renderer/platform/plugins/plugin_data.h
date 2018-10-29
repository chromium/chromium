
/*
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PLUGINS_PLUGIN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PLUGINS_PLUGIN_DATA_H_

#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PluginInfo;

class PLATFORM_EXPORT MimeClassInfo final
    : public GarbageCollectedFinalized<MimeClassInfo> {
 public:
  void Trace(blink::Visitor*);

  MimeClassInfo(const String& type, const String& desc, PluginInfo&);

  const String& Type() const { return type_; }
  const String& Description() const { return description_; }
  const Vector<String>& Extensions() const { return extensions_; }
  const PluginInfo* Plugin() const { return plugin_; }

 private:
  friend class PluginData;
  friend class PluginListBuilder;

  String type_;
  String description_;
  Vector<String> extensions_;
  Member<PluginInfo> plugin_;
};

class PLATFORM_EXPORT PluginInfo final
    : public GarbageCollectedFinalized<PluginInfo> {
 public:
  void Trace(blink::Visitor*);

  PluginInfo(const String& name,
             const String& filename,
             const String& desc,
             Color background_color);

  void AddMimeType(MimeClassInfo*);

  const HeapVector<Member<MimeClassInfo>>& Mimes() const { return mimes_; }
  const MimeClassInfo* GetMimeClassInfo(wtf_size_t index) const;
  const MimeClassInfo* GetMimeClassInfo(const String& type) const;
  wtf_size_t GetMimeClassInfoSize() const;

  const String& Name() const { return name_; }
  const String& Filename() const { return filename_; }
  const String& Description() const { return description_; }
  Color BackgroundColor() const { return background_color_; }

 private:
  friend class MimeClassInfo;
  friend class PluginData;
  friend class PluginListBuilder;

  String name_;
  String filename_;
  String description_;
  Color background_color_;
  HeapVector<Member<MimeClassInfo>> mimes_;
};

class PLATFORM_EXPORT PluginData final
    : public GarbageCollectedFinalized<PluginData> {
  WTF_MAKE_NONCOPYABLE(PluginData);

 public:
  void Trace(blink::Visitor*);

  static PluginData* Create() { return new PluginData(); }

  const HeapVector<Member<PluginInfo>>& Plugins() const { return plugins_; }
  const HeapVector<Member<MimeClassInfo>>& Mimes() const { return mimes_; }
  const SecurityOrigin* Origin() const { return main_frame_origin_.get(); }
  void UpdatePluginList(const SecurityOrigin* main_frame_origin);
  void ResetPluginData();

  bool SupportsMimeType(const String& mime_type) const;
  Color PluginBackgroundColorForMimeType(const String& mime_type) const;

  // refreshBrowserSidePluginCache doesn't update existent instances of
  // PluginData.
  static void RefreshBrowserSidePluginCache();

 private:
  PluginData() = default;

  HeapVector<Member<PluginInfo>> plugins_;
  HeapVector<Member<MimeClassInfo>> mimes_;
  scoped_refptr<const SecurityOrigin> main_frame_origin_;
};

}  // namespace blink

#endif
