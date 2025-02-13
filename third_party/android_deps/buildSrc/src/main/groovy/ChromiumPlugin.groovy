// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.api.artifacts.DependencyResolveDetails
import org.gradle.api.attributes.Attribute
import org.gradle.api.attributes.AttributeCompatibilityRule
import org.gradle.api.attributes.CompatibilityCheckDetails
import org.gradle.api.attributes.java.TargetJvmEnvironment

/**
 * Plugin designed to define the configuration names to be used in the Gradle files to describe the dependencies that
 * {@link ChromiumDepGraph} with pick up.
 */
class ChromiumPlugin implements Plugin<Project> {
    // Do not fail if environment != android
    static class TargetJvmEnvironmentCompatibilityRules implements AttributeCompatibilityRule<TargetJvmEnvironment> {

        // public constructor to make reflective initialization happy.
        TargetJvmEnvironmentCompatibilityRules() {}

        @Override
        void execute(CompatibilityCheckDetails<TargetJvmEnvironment> details) {
            // This means regardless of the actual value of the attribute, it is
            // considered a match. Gradle still picks the closest though if multiple
            // options are available (which is what we want).
            details.compatible()
        }
    }

    void apply(Project project) {
        // The configurations here are going to be used in ChromiumDepGraph. Keep it up to date with the declarations
        // below.
        project.configurations {
            /** Main type of configuration, use it for libraries that the APK depends on. */
            compile

            /** Same as compile, but uses the latest versions of androidx deps. */
            compileLatest

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

            /** Same as androidTestCompile, but uses the latest versions of androidx deps. */
            androidTestCompileLatest
        }

        project.dependencies.attributesSchema {
            attribute(TargetJvmEnvironment.TARGET_JVM_ENVIRONMENT_ATTRIBUTE) {
                compatibilityRules.add(TargetJvmEnvironmentCompatibilityRules.class)
            }
        }

        project.configurations.configureEach {
            attributes {
                attribute(Attribute.of("org.gradle.category", String), "library")
                attribute(Attribute.of("org.gradle.usage", String), "java-runtime")
                attribute(TargetJvmEnvironment.TARGET_JVM_ENVIRONMENT_ATTRIBUTE,
                        project.objects.named(TargetJvmEnvironment, TargetJvmEnvironment.ANDROID))
            }
        }

        project.configurations.compileLatest {
            resolutionStrategy.eachDependency { DependencyResolveDetails details ->
                overrideVersionIfNecessary(details)
            }
        }

        project.configurations.androidTestCompileLatest {
            resolutionStrategy.eachDependency { DependencyResolveDetails details ->
                overrideVersionIfNecessary(details)
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

        project.configurations.buildCompileNoDeps {
            // transitive false means do not also pull in the deps of these deps.
            transitive = false
        }

    }

    private static void overrideVersionIfNecessary(DependencyResolveDetails details) {
        String group = details.requested.group
        String version = details.requested.version
        if (group.startsWith('androidx') && version != '+' && !version.contains('-SNAPSHOT')) {
            details.useVersion '+'
        }
    }

}
