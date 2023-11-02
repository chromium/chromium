// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Build;
import android.view.ViewStructure;

import androidx.annotation.RequiresApi;

import org.chromium.content.browser.RenderCoordinatesImpl;

/**
 */
public class OViewStructureBuilder extends ViewStructureBuilder {
    public OViewStructureBuilder(RenderCoordinatesImpl renderCoordinates) {
        super(renderCoordinates);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @Override
    protected void setViewStructureNodeHtmlInfo(
            ViewStructure node, String htmlTag, String cssDisplay, String[][] htmlAttributes) {
        super.setViewStructureNodeHtmlInfo(node, htmlTag, cssDisplay, htmlAttributes);

        ViewStructure.HtmlInfo.Builder htmlBuilder = node.newHtmlInfoBuilder(htmlTag);
        if (htmlBuilder != null) {
            htmlBuilder.addAttribute("display", cssDisplay);
            for (String[] attr : htmlAttributes) {
                htmlBuilder.addAttribute(attr[0], attr[1]);
            }
            node.setHtmlInfo(htmlBuilder.build());
        }
    }
}
