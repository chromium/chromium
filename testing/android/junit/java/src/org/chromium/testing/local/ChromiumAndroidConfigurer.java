// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.robolectric.annotation.Config;
import org.robolectric.config.AndroidConfigurer;
import org.robolectric.internal.bytecode.InstrumentationConfiguration;
import org.robolectric.internal.bytecode.ShadowProviders;

import org.chromium.build.annotations.ServiceImpl;

import java.util.Optional;
import java.util.ServiceLoader;

/**
 * Tells Robolectric which classes to exclude from its sandbox. This is required to avoid the need
 * to create a new Robolectric ClassLoader for each distinct set of Shadows.
 */
@ServiceImpl(AndroidConfigurer.class)
public class ChromiumAndroidConfigurer extends AndroidConfigurer {
    public interface ExtraConfiguration {
        void withConfig(InstrumentationConfiguration.Builder builder, Config config);
    }

    private static JSONObject sConfigJson;
    private Optional<ExtraConfiguration> mExtraConfig;

    static void setJsonConfig(JSONObject root) {
        sConfigJson = root;
    }

    public ChromiumAndroidConfigurer(ShadowProviders shadowProviders) {
        super(shadowProviders);
        mExtraConfig = ServiceLoader.load(ExtraConfiguration.class).findFirst();
    }

    @Override
    public void withConfig(InstrumentationConfiguration.Builder builder, Config config) {
        super.withConfig(builder, config);
        try {
            JSONArray instrumentedPackages = sConfigJson.getJSONArray("instrumentedPackages");
            for (int i = 0, len = instrumentedPackages.length(); i < len; ++i) {
                builder.addInstrumentedPackage(instrumentedPackages.getString(i));
            }
            JSONArray instrumentedClasses = sConfigJson.getJSONArray("instrumentedClasses");
            for (int i = 0, len = instrumentedClasses.length(); i < len; ++i) {
                builder.addInstrumentedClass(instrumentedClasses.getString(i));
            }
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
        if (mExtraConfig.isPresent()) {
            mExtraConfig.get().withConfig(builder, config);
        }
    }
}
