// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.api.artifacts.DependencyResolveDetails
import org.gradle.api.attributes.*
import org.gradle.api.attributes.java.TargetJvmEnvironment

/**
 * Plugin designed to define the configuration names to be used in the Gradle files to describe the dependencies that
 * {@link ChromiumDepGraph} with pick up.
 */
class ChromiumPlugin implements Plugin<Project> {
    // Do not fail if environment != android
    static class TargetJvmEnvironmentCompatibilityRules implements AttributeCompatibilityRule<TargetJvmEnvironment> {

        // public constructor to make reflective initialization happy.
        public TargetJvmEnvironmentCompatibilityRules() {}

        @Override
        public void execute(CompatibilityCheckDetails<TargetJvmEnvironment> details) {
            // This means regardless of the actual value of the attribute, it is
            // considered a match. Gradle still picks the closest though if multiple
            // options are available (which is what we want).
            details.compatible();
        }
    }

    void apply(Project project) {
        // The configurations here are going to be used in ChromiumDepGraph. Keep it up to date with the declarations
        // below.
        project.configurations {
            /** Main type of configuration, use it for libraries that the APK depends on. */
            compile

            /**
             * Dedicated com_google_guava_listenablefuture configuration so that other libraries do not affect the
             * resolved listenablefuture version.
             */
            compileListenableFuture

            /** Libraries that are for testing only. */
            testCompile

            /** Libraries that are only used during build. These support android. */
            buildCompile

            /** Libraries that are only used during build but should not automatically retrieve their dependencies. */
            buildCompileNoDeps

            /** Libraries that are used for testing only and support android. */
            androidTestCompile
        }

        project.dependencies.attributesSchema {
            attribute(TargetJvmEnvironment.TARGET_JVM_ENVIRONMENT_ATTRIBUTE) {
                getCompatibilityRules().add(TargetJvmEnvironmentCompatibilityRules.class)
            }
        }

        project.configurations.all {
            resolutionStrategy.eachDependency { DependencyResolveDetails details ->
                if (project.ext.has('versionOverrideMap') && project.ext.versionOverrideMap) {
                    String module = "${details.requested.group}:${details.requested.name}"
                    String version = project.ext.versionOverrideMap[module]
                    if (version != null) {
                        details.useVersion version
                    }
                }
            }
            attributes {
                attribute(Attribute.of("org.gradle.category", String), "library")
                attribute(Attribute.of("org.gradle.usage", String), "java-runtime")
                attribute(TargetJvmEnvironment.TARGET_JVM_ENVIRONMENT_ATTRIBUTE,
                        project.objects.named(TargetJvmEnvironment, TargetJvmEnvironment.ANDROID))
            }
        }

        // testCompile config is for host side tests (Robolectric) so we prefer
        // the non-android versions of deps if available.
        project.configurations.testCompile {
            attributes {
                attribute(TargetJvmEnvironment.TARGET_JVM_ENVIRONMENT_ATTRIBUTE,
                        project.objects.named(TargetJvmEnvironment, TargetJvmEnvironment.STANDARD_JVM))
            }
        }

        project.configurations.buildCompile {
            attributes {
                // This attribute is used to resolve the caffeine error in: https://crbug.com/1216032#c3
                attribute(Bundling.BUNDLING_ATTRIBUTE, project.objects.named(Bundling, Bundling.EXTERNAL))
            }
        }
        project.configurations.buildCompileNoDeps {
            // transitive false means do not also pull in the deps of these deps.
            transitive = false
        }

    }

}
