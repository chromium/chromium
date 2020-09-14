/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Google, Inc.
 * All rights reserved.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_THEME_PAINTER_DEFAULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_THEME_PAINTER_DEFAULT_H_

#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"

namespace blink {

class LayoutThemeDefault;

class ThemePainterDefault final : public ThemePainter {
 public:
  explicit ThemePainterDefault(LayoutThemeDefault&);

 private:
  bool PaintCheckbox(const Element&,
                     const Document&,
                     const ComputedStyle&,
                     const PaintInfo&,
                     const IntRect&) override;
  bool PaintRadio(const Element&,
                  const Document&,
                  const ComputedStyle&,
                  const PaintInfo&,
                  const IntRect&) override;
  bool PaintButton(const Element&,
                   const Document&,
                   const ComputedStyle&,
                   const PaintInfo&,
                   const IntRect&) override;
  bool PaintTextField(const Element&,
                      const ComputedStyle&,
                      const PaintInfo&,
                      const IntRect&) override;
  bool PaintMenuList(const Element&,
                     const Document&,
                     const ComputedStyle&,
                     const PaintInfo&,
                     const IntRect&) override;
  bool PaintMenuListButton(const Element&,
                           const Document&,
                           const ComputedStyle&,
                           const PaintInfo&,
                           const IntRect&) override;
  bool PaintSliderTrack(const Element& element,
                        const LayoutObject&,
                        const PaintInfo&,
                        const IntRect&) override;
  bool PaintSliderThumb(const Element&,
                        const ComputedStyle&,
                        const PaintInfo&,
                        const IntRect&) override;
  bool PaintInnerSpinButton(const Element&,
                            const ComputedStyle&,
                            const PaintInfo&,
                            const IntRect&) override;
  bool PaintProgressBar(const Element& element,
                        const LayoutObject&,
                        const PaintInfo&,
                        const IntRect&) override;
  bool PaintTextArea(const Element&,
                     const ComputedStyle&,
                     const PaintInfo&,
                     const IntRect&) override;
  bool PaintSearchField(const Element&,
                        const ComputedStyle&,
                        const PaintInfo&,
                        const IntRect&) override;
  bool PaintSearchFieldCancelButton(const LayoutObject&,
                                    const PaintInfo&,
                                    const IntRect&) override;

  void SetupMenuListArrow(const Document&,
                          const ComputedStyle&,
                          const IntRect&,
                          WebThemeEngine::ExtraParams&);

  // ThemePaintDefault is a part object of m_theme.
  LayoutThemeDefault& theme_;
};

}  // namespace blink

#endif  // ThemePainerDefault_h
