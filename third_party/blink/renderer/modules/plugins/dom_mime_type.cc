/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 */

#include "third_party/blink/renderer/modules/plugins/dom_mime_type.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/plugins/dom_plugin.h"
#include "third_party/blink/renderer/modules/plugins/dom_plugin_array.h"
#include "third_party/blink/renderer/modules/plugins/navigator_plugins.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

DOMMimeType::DOMMimeType(LocalDOMWindow* window,
                         const MimeClassInfo& mime_class_info)
    : window_(window), mime_class_info_(&mime_class_info) {}

void DOMMimeType::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(mime_class_info_);
  ScriptWrappable::Trace(visitor);
}

const String& DOMMimeType::type() const {
  return mime_class_info_->Type();
}

String DOMMimeType::suffixes() const {
  const Vector<String>& extensions = mime_class_info_->Extensions();

  StringBuilder builder;
  builder.AppendRange(extensions, ",");
  return builder.ReleaseString();
}

const String& DOMMimeType::description() const {
  return mime_class_info_->Description();
}

DOMPlugin* DOMMimeType::enabledPlugin() const {
  // FIXME: allowPlugins is just a client call. We should not need
  // to bounce through the loader to get there.
  // Something like: frame()->page()->client()->allowPlugins().
  if (!window_ || !window_->GetFrame() ||
      !window_->GetFrame()->Loader().AllowPlugins()) {
    return nullptr;
  }

  return NavigatorPlugins::plugins(*window_->navigator())
      ->namedItem(AtomicString(mime_class_info_->Plugin()->Name()));
}

}  // namespace blink
