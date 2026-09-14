// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import android.graphics.Typeface;
import android.os.Build;
import android.text.Hyphenator;

import org.robolectric.RuntimeEnvironment;
import org.robolectric.nativeruntime.DefaultNativeRuntimeLoader;
import org.robolectric.pluginapi.NativeRuntimeLoader;
import org.robolectric.util.OsUtil;
import org.robolectric.util.PerfStatsCollector;
import org.robolectric.util.inject.Supersedes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;

import java.io.File;
import java.util.Arrays;
import java.util.Locale;

/**
 * Custom NativeRuntimeLoader that loads pre-extracted native runtime libraries and resource data
 * when available, avoiding multi-second per-shard ZIP extraction.
 */
@NullMarked
@ServiceImpl(NativeRuntimeLoader.class)
@Supersedes(DefaultNativeRuntimeLoader.class)
public class ChromiumNativeRuntimeLoader extends DefaultNativeRuntimeLoader {
    private static final String EXTRACTED_DIR_PROPERTY = "chromium.robolectric.extractedDir";

    @Override
    public synchronized void ensureLoaded() {
        if (loaded.get()) {
            return;
        }

        String extractedDirPath = System.getProperty(EXTRACTED_DIR_PROPERTY);
        if (extractedDirPath == null) {
            super.ensureLoaded();
            return;
        }

        File extractedBaseDir = new File(extractedDirPath);
        File compatDir = findCompatDir(extractedBaseDir);

        File sdkDir =
                !isAndroidVOrGreater()
                        ? null
                        : findSdkDir(extractedBaseDir, RuntimeEnvironment.getApiLevel());

        loaded.set(true);
        PerfStatsCollector.getInstance()
                .measure("loadNativeRuntime", () -> loadFromExtracted(compatDir, sdkDir));
    }

    private void loadFromExtracted(File compatDir, @Nullable File sdkDir) {
        String fontsDir = new File(compatDir, "fonts").getAbsolutePath() + File.separator;
        System.setProperty("robolectric.nativeruntime.fontdir", fontsDir);
        setIcuDataPath(sdkDir != null ? sdkDir : compatDir);

        if (sdkDir != null) {
            // No need to reset these since we always shard based on API level.
            System.setProperty("use_base_native_hostruntime", "true");
            System.setProperty("core_native_classes", String.join(",", getCoreClassNatives()));
            System.setProperty("graphics_native_classes", String.join(",", getGraphicsNatives()));
            System.setProperty("method_binding_format", METHOD_BINDING_FORMAT);

            System.load(findNativeLib(sdkDir));

            invokeDeferredStaticInitializers();
            File hyphenDir = new File(sdkDir, "hyphen-data");
            if (hyphenDir.isDirectory()) {
                setNativeSystemProperty("ro.hyphen.data.dir", hyphenDir.getAbsolutePath());
            }
            Typeface.loadPreinstalledSystemFontMap();
        } else {
            File hyphenDir = new File(compatDir, "hyphen-data");
            if (hyphenDir.isDirectory()) {
                System.setProperty("hyphen.data.dir", hyphenDir.getAbsolutePath());
            }

            System.load(findNativeLib(compatDir));
        }

        if (RuntimeEnvironment.getApiLevel() >= Build.VERSION_CODES.P) {
            Hyphenator.init();
        }
    }

    private static void setIcuDataPath(File dir) {
        File icuDir = new File(dir, "icu");
        File[] icuFiles =
                icuDir.listFiles((d, name) -> name.startsWith("icudt") && name.endsWith(".dat"));
        if (icuFiles != null && icuFiles.length > 0) {
            Arrays.sort(icuFiles);
            System.setProperty("icu.data.path", icuFiles[0].getAbsolutePath());
            System.setProperty("icu.locale.default", Locale.getDefault().toLanguageTag());
        }
    }

    private static String findNativeLib(File dir) {
        String libName = libraryName();
        File libFile = new File(dir, nativeLibraryResourcePath(libName));
        if (libFile.exists()) {
            return libFile.getAbsolutePath();
        }
        throw new IllegalStateException("Native library not found: " + libFile);
    }

    private static String nativeLibraryResourcePath(String libName) {
        String os = OsUtil.isLinux() ? "linux" : OsUtil.isMac() ? "mac" : "windows";
        String arch = System.getProperty("os.arch").toLowerCase(Locale.US);
        if (arch.equals("amd64")) {
            arch = "x86_64";
        } else if (arch.equals("arm64")) {
            arch = "aarch64";
        }
        return String.format("native/%s/%s/%s", os, arch, libName);
    }

    private static File findCompatDir(File extractedBaseDir) {
        File[] subdirs = extractedBaseDir.listFiles();
        if (subdirs != null) {
            for (File subdir : subdirs) {
                if (subdir.isDirectory()
                        && subdir.getName().startsWith("nativeruntime-dist-compat")) {
                    return subdir;
                }
            }
        }
        throw new IllegalStateException("No nativeruntime-dist-compat within " + extractedBaseDir);
    }

    private static File findSdkDir(File extractedBaseDir, int apiLevel) {
        // For Android V+ (API 35+), Robolectric names jars using the release version
        // (apiLevel - 20), e.g. "android-all-instrumented-15-...".
        String prefix = "android-all-instrumented-" + (apiLevel - 20) + "-";
        File[] subdirs = extractedBaseDir.listFiles();
        if (subdirs != null) {
            for (File subdir : subdirs) {
                if (subdir.isDirectory() && subdir.getName().startsWith(prefix)) {
                    return subdir;
                }
            }
        }
        throw new IllegalStateException("No \"" + prefix + "\" within " + extractedBaseDir);
    }

    private static boolean isAndroidVOrGreater() {
        return RuntimeEnvironment.getApiLevel() >= Build.VERSION_CODES.VANILLA_ICE_CREAM;
    }
}
