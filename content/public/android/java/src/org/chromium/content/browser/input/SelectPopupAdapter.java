// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.CheckedTextView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

/**
 * Select popup item adapter for SelectPopupDialog, used so we can disable
 * OPTION_GROUP items.
 */
public class SelectPopupAdapter extends ArrayAdapter<SelectPopupItem> {
    // Holds the items of the select popup alert dialog list.
    private List<SelectPopupItem> mItems;

    // True if all items have type PopupItemType.ENABLED.
    private boolean mAreAllItemsEnabled;

    /**
     * Creates a new SelectPopupItem adapter for the select popup alert dialog list.
     * @param context        Application context.
     * @param layoutResource Layout resource used for the alert dialog list.
     * @param items          SelectPopupItem array list.
     */
    public SelectPopupAdapter(Context context, int layoutResource, List<SelectPopupItem> items) {
        super(context, layoutResource, items);
        mItems = new ArrayList<SelectPopupItem>(items);

        mAreAllItemsEnabled = true;
        for (int i = 0; i < mItems.size(); i++) {
            if (mItems.get(i).getType() != PopupItemType.ENABLED) {
                mAreAllItemsEnabled = false;
                break;
            }
        }
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (position < 0 || position >= getCount()) return null;

        convertView = super.getView(position, convertView, parent);
        ((TextView) convertView).setText(mItems.get(position).getLabel());

        // Currently select_dialog_(single|multi)choice uses CheckedTextViews.
        // If that changes, the class cast will no longer be valid.
        // The WebView build cannot rely on this being the case, so
        // we must check.
        if (convertView instanceof CheckedTextView) {
            // <optgroup> elements do not have check marks. If an item previously used as an
            // <optgroup> gets reused for a non-<optgroup> element, we need to get the check mark
            // back. Inflating a new View from XML can be slow, for both the inflation part and GC
            // afterwards. Even creating a new Drawable can be tricky, considering getting the
            // check/radio type and theme right.
            // Saving the previously removed Drawables and reuse them when needed is faster,
            // and the memory implication should be fine.
            CheckedTextView view = (CheckedTextView) convertView;
            if (mItems.get(position).getType() == PopupItemType.GROUP) {
                if (view.getCheckMarkDrawable() != null) {
                    view.setTag(view.getCheckMarkDrawable());
                    view.setCheckMarkDrawable(null);
                }
            } else {
                if (view.getCheckMarkDrawable() == null) {
                    view.setCheckMarkDrawable((Drawable) view.getTag());
                }
            }
        }
        // Draw the disabled element in a disabled state.
        convertView.setEnabled(mItems.get(position).getType() != PopupItemType.DISABLED);

        return convertView;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return mAreAllItemsEnabled;
    }

    @Override
    public boolean isEnabled(int position) {
        if (position < 0 || position >= getCount()) return false;
        return mItems.get(position).getType() == PopupItemType.ENABLED;
    }
}
