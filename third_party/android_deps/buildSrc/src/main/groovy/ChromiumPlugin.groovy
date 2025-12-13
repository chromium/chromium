// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.api.artifacts.*
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
            /** Main type of configuration, use it for libraries that the APK depends on.
             * requires_android set to true. */
            compile
            compileLatest

            /** Same as compile but just supports_android.  */
            supportsAndroidCompile
            supportsAndroidCompileLatest

            /** Libraries that are for testing only. */
            testCompile
            testCompileLatest

            /** Libraries that are only used during build. */
            buildCompile
            buildCompileLatest

            /** Libraries that are only used during build but should not automatically retrieve their dependencies. */
            buildCompileNoDeps

            /** Libraries that are used for testing only and support android. */
            androidTestCompile
            androidTestCompileLatest
        }

        def constraintAndroid = {
            attribute(TargetJvmEnvironment.TARGET_JVM_ENVIRONMENT_ATTRIBUTE,
                    project.objects.named(TargetJvmEnvironment, TargetJvmEnvironment.ANDROID))
        }
        def constraintJvm = {
            attribute(TargetJvmEnvironment.TARGET_JVM_ENVIRONMENT_ATTRIBUTE,
                    project.objects.named(TargetJvmEnvironment, TargetJvmEnvironment.STANDARD_JVM))
        }

        project.dependencies.attributesSchema {
            attribute(TargetJvmEnvironment.TARGET_JVM_ENVIRONMENT_ATTRIBUTE) {
                compatibilityRules.add(TargetJvmEnvironmentCompatibilityRules.class)
            }
        }

        // Helps gradle disambiguate between different variants of the same
        // dependency. We usually want the same thing for all deps, a java
        // implementation not an API, source files, docs, etc. Sometimes we want
        // the android variant if available and sometimes the desktop JVM
        // variant depending on the configuration.
        project.configurations.configureEach { Configuration config ->
            attributes {
                attribute(Attribute.of("org.gradle.category", String), "library")
                attribute(Attribute.of("org.gradle.usage", String), "java-runtime")
            }
            // testCompile config is for host side tests (Robolectric) so we prefer
            // the non-android versions of deps if available.
            if (config.name.startsWith('testCompile')) {
                attributes constraintJvm
            } else {
                attributes constraintAndroid
            }
        }

        def latestResolutionStrategy = {
            if (project.hasProperty('versionCache') && project.versionCache) {
                project.ext.versionCache.each { String selector, String version ->
                    force "${selector}:${version}"
                }
            } else {
                eachDependency { DependencyResolveDetails details ->
                    overrideVersionIfNecessary(project, details)
                }
            }
        }

        project.configurations.each { Configuration configuration ->
            if (configuration.name.endsWith('Latest')) {
                configuration.resolutionStrategy(latestResolutionStrategy)
            }
        }

        project.configurations.buildCompileNoDeps {
            // transitive false means do not also pull in the deps of these deps.
            transitive = false
        }

    }

    private static void overrideVersionIfNecessary(Project project, DependencyResolveDetails details) {
        String group = details.requested.group
        String requestedVersion = details.requested.version
        if (group.startsWith('androidx') && requestedVersion != '+' && !requestedVersion.contains('-SNAPSHOT')) {
            details.useVersion '+'
        }
    }

}
