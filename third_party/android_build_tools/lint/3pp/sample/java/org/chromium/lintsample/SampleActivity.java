// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.lintsample;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;
import androidx.annotation.UiThread;
import androidx.annotation.WorkerThread;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.recyclerview.widget.RecyclerView;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;

/**
 * Not compiled: a lint input that triggers common checks (and androidx ones) while lint runs under
 * the GraalVM tracing agent.
 */
public class SampleActivity extends FragmentActivity {
    @IntDef({MODE_A, MODE_B})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Mode {}

    public static final int MODE_A = 0;
    public static final int MODE_B = 1;

    private final Handler mHandler = new Handler();
    private final List<String> mItems = new ArrayList<>();
    private @Mode int mMode;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.sample);
        TextView text = findViewById(R.id.sample_text);
        text.setText(String.format("%d items", mItems.size()));
        text.setText(getString(R.string.sample_format, 3));
        setMode(2);
        SharedPreferences prefs = getSharedPreferences("sample", Context.MODE_PRIVATE);
        prefs.edit().putInt("count", 1);
        Log.d("SampleActivity", "mode " + mMode);
        if (Build.VERSION.SDK_INT >= 21) {
            text.setElevation(1f);
        }
        if (Build.VERSION.SDK_INT >= 35) {
            newApi();
        }
        getSupportFragmentManager()
                .beginTransaction()
                .detach(new SampleFragment())
                .attach(new SampleFragment())
                .commit();
        try {
            Field field = Class.forName("android.app.ActivityThread").getDeclaredField("mH");
            field.setAccessible(true);
        } catch (ReflectiveOperationException e) {
            Log.e("SampleActivity", "reflection", e);
        }
        mHandler.post(
                new Runnable() {
                    @Override
                    public void run() {
                        text.setVisibility(View.GONE);
                    }
                });
        new Thread(this::loadInBackground).start();
    }

    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    private void newApi() {
        getWindow().setNavigationBarContrastEnforced(false);
    }

    void setMode(@Mode int mode) {
        mMode = mode;
    }

    @WorkerThread
    void loadInBackground() {
        updateUi();
    }

    @UiThread
    void updateUi() {
        mItems.add("item");
    }

    /** A fragment that observes LiveData with the wrong lifecycle owner. */
    public static class SampleFragment extends Fragment {
        @Override
        public void onViewCreated(View view, Bundle savedInstanceState) {
            LiveData<String> data = new MutableLiveData<>();
            data.observe(this, value -> Log.i("SampleFragment", value));
        }
    }

    /** An adapter that refreshes everything. */
    public static class SampleAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        private final List<String> mData = new ArrayList<>();

        void replace(List<String> data) {
            mData.clear();
            mData.addAll(data);
            notifyDataSetChanged();
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(android.view.ViewGroup parent, int type) {
            return new RecyclerView.ViewHolder(new TextView(parent.getContext())) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
            ((TextView) holder.itemView).setText(mData.get(position));
        }

        @Override
        public int getItemCount() {
            return mData.size();
        }
    }

    /** Unused activity subclass with a non-default constructor. */
    public static class LeakyActivity extends Activity {
        static Context sContext;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            sContext = this;
        }
    }
}
