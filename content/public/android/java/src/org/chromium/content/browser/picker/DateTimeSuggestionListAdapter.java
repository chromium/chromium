// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.R;

import java.util.List;

/** Date/time suggestion adapter for the suggestion dialog. */
@NullMarked
class DateTimeSuggestionListAdapter extends ArrayAdapter<DateTimeSuggestion> {
    private final Context mContext;

    DateTimeSuggestionListAdapter(Context context, List<DateTimeSuggestion> objects) {
        super(context, R.layout.date_time_suggestion, objects);
        mContext = context;
    }

    @Override
    public View getView(int position, @Nullable View convertView, ViewGroup parent) {
        View layout;
        if (convertView != null) {
            layout = convertView;
        } else {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            layout = inflater.inflate(R.layout.date_time_suggestion, parent, false);
        }
        TextView labelView = layout.findViewById(R.id.date_time_suggestion_value);
        TextView sublabelView = layout.findViewById(R.id.date_time_suggestion_label);

        if (position == getCount() - 1) {
            labelView.setText(mContext.getText(R.string.date_picker_dialog_other_button_label));
            sublabelView.setText("");
        } else {
            DateTimeSuggestion item = assumeNonNull(getItem(position));
            labelView.setText(item.localizedValue());
            sublabelView.setText(item.label());
        }

        return layout;
    }

    @Override
    public int getCount() {
        return super.getCount() + 1;
    }
}
