// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import org.gradle.api.Plugin
import org.gradle.api.Project

/**
 * Plugin designed to define the configuration names to be used in the Gradle files to describe
 * the dependencies that {@link ChromiumDepGraph} with pick up.
 */
class ChromiumPlugin implements Plugin<Project> {
    void apply(Project project) {
        // The configurations here are going to be used in ChromiumDepGraph. Keep it up to date
        // with the declarations below.
        project.configurations {
            /** Main type of configuration, use it for libraries that the APK depends on. */
            compile

            /**
             * Dedicated com_google_guava_listenablefuture configuration so that other libraries do
             * not affect the resolved listenablefuture version.
             */
            compileListenableFuture

            /** Libraries that are for testing only. */
            testCompile

            /** Libraries that are only used during build. These support android. */
            buildCompile

            /**
             * Libraries that are only used during build but should not
             * automatically retrieve their dependencies.
             */
            buildCompileNoDeps

            /** Libraries that are used for testing only and support android. */
            androidTestCompile
        }
    }
}
