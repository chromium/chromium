// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import groovy.util.slurpersupport.GPathResult
import org.gradle.api.Project
import org.gradle.api.artifacts.ResolvedArtifact
import org.gradle.api.artifacts.ResolvedConfiguration
import org.gradle.api.artifacts.ResolvedDependency
import org.gradle.api.artifacts.ResolvedModuleVersion
import org.gradle.api.artifacts.component.ComponentIdentifier
import org.gradle.api.artifacts.result.ResolvedArtifactResult
import org.gradle.maven.MavenModule
import org.gradle.maven.MavenPomArtifact

/**
 * Parses the project dependencies and generates a graph of
 * {@link ChromiumDepGraph.DependencyDescription} objects to make the data manipulation easier.
 */
class ChromiumDepGraph {
    final def dependencies = new HashMap<String, DependencyDescription>()

    // Override to use the lower version of the library when
    // resolving which library version to use.
    final def LOWER_VERSION_OVERRIDE = [
         'com_google_guava_listenablefuture',
    ]
    // Some libraries don't properly fill their POM with the appropriate licensing information.
    // It is provided here from manual lookups. Note that licenseUrl must provide textual content
    // rather than be an html page.
    final def PROPERTY_OVERRIDES = [
        'androidx_multidex_multidex': new PropertyOverride(
            url: 'https://maven.google.com/androidx/multidex/multidex/2.0.0/multidex-2.0.0.aar'),
        'com_android_tools_desugar_jdk_libs': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/google/desugar_jdk_libs/master/LICENSE",
            generateTarget: false),
        'com_android_tools_desugar_jdk_libs_configuration': new PropertyOverride(
            licensePath: "licenses/desugar_jdk_libs_configuration.txt",
            licenseName: "BSD 3-Clause",
            generateTarget: false),
        'backport_util_concurrent_backport_util_concurrent': new PropertyOverride(
            licensePath: "licenses/CC01.0.txt",
            licenseName: "CC0 1.0"),
        'classworlds_classworlds': new PropertyOverride(
            description: "A class loader framework.",
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'com_github_kevinstern_software_and_algorithms': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/KevinStern/software-and-algorithms/master/LICENSE"),
        'com_google_auto_auto_common': new PropertyOverride(
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_auto_service_auto_service': new PropertyOverride(
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_auto_service_auto_service_annotations': new PropertyOverride(
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_auto_value_auto_value_annotations': new PropertyOverride(
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_code_findbugs_jFormatString': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/spotbugs/spotbugs/master/spotbugs/licenses/LICENSE.txt"),
        'com_google_code_gson_gson': new PropertyOverride(
            url: "https://github.com/google/gson",
            licenseUrl: "https://raw.githubusercontent.com/google/gson/master/LICENSE",
            licenseName: "Apache 2.0"),
        'com_google_errorprone_error_prone_annotation': new PropertyOverride(
            url: "https://errorprone.info/",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt"),
        'com_google_errorprone_error_prone_annotations': new PropertyOverride(
            url: "https://errorprone.info/",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_googlejavaformat_google_java_format': new PropertyOverride(
            url: "https://github.com/google/google-java-format",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_guava_failureaccess': new PropertyOverride(
            url: "https://github.com/google/guava",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_guava_guava': new PropertyOverride(
            url: "https://github.com/google/guava",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_guava_guava_android': new PropertyOverride(
            url: "https://github.com/google/guava",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_guava_listenablefuture': new PropertyOverride(
            url: "https://github.com/google/guava",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'nekohtml_nekohtml': new PropertyOverride(
            description: "NekoHTML is a simple HTML scanner and tag balancer."),
        'nekohtml_xercesMinimal': new PropertyOverride(
            description: "Only contains necessary framework & Xerces2 classes",
            url: 'http://nekohtml.sourceforge.net/index.html',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_ant_ant': new PropertyOverride(
            url: 'https://ant.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_ant_ant_launcher': new PropertyOverride(
            url: 'https://ant.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_ant_tasks': new PropertyOverride(
            url: 'https://ant.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_artifact': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_artifact_manager': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_error_diagnostics': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_model': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_plugin_registry': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_profile': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_project': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_repository_metadata': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_maven_settings': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_wagon_wagon_file': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_wagon_wagon_http_lightweight': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_wagon_wagon_http_shared': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_apache_maven_wagon_wagon_provider_api': new PropertyOverride(
            url: 'https://maven.apache.org/',
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_codehaus_mojo_animal_sniffer_annotations': new PropertyOverride(
            url: "http://www.mojohaus.org/animal-sniffer/animal-sniffer-annotations/",
            licenseUrl: "https://raw.githubusercontent.com/mojohaus/animal-sniffer/master/animal-sniffer-annotations/pom.xml",
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_codehaus_plexus_plexus_container_default': new PropertyOverride(
            url: "https://codehaus-plexus.github.io/",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_codehaus_plexus_plexus_interpolation': new PropertyOverride(
            url: "https://codehaus-plexus.github.io/",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'org_codehaus_plexus_plexus_utils': new PropertyOverride(
            url: "https://codehaus-plexus.github.io/",
            licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
            licenseName: "Apache 2.0"),
        'com_google_protobuf_protobuf_java': new PropertyOverride(
            url: "https://github.com/protocolbuffers/protobuf/blob/master/java/README.md",
            licenseUrl: "https://raw.githubusercontent.com/protocolbuffers/protobuf/master/LICENSE",
            licenseName: "BSD"),
        'com_google_protobuf_protobuf_javalite': new PropertyOverride(
            url: "https://github.com/protocolbuffers/protobuf/blob/master/java/lite.md",
            licenseUrl: "https://raw.githubusercontent.com/protocolbuffers/protobuf/master/LICENSE",
            licenseName: "BSD"),
        'com_google_protobuf_protobuf_lite': new PropertyOverride(
            url: "https://github.com/protocolbuffers/protobuf/blob/master/java/lite.md",
            licenseUrl: "https://raw.githubusercontent.com/protocolbuffers/protobuf/master/LICENSE",
            licenseName: "BSD"),
        'com_google_ar_core': new PropertyOverride(
            url: "https://github.com/google-ar/arcore-android-sdk",
            licenseUrl: "https://raw.githubusercontent.com/google-ar/arcore-android-sdk/master/LICENSE",
            licenseName: "Apache 2.0"),
        'commons_cli_commons_cli': new PropertyOverride(
            licenseName: "Apache 2.0",
            licenseUrl: "https://raw.githubusercontent.com/apache/commons-cli/master/LICENSE.txt"),
        'javax_annotation_javax_annotation_api': new PropertyOverride(
            isShipped: false,  // Annotations are stripped by R8.
            licenseName: "CDDLv1.1",
            licensePath: "licenses/CDDLv1.1.txt"),
        'javax_annotation_jsr250_api': new PropertyOverride(
            isShipped: false,  // Annotations are stripped by R8.
            licenseName: "CDDLv1.0",
            licensePath: "licenses/CDDLv1.0.txt"),
        'net_sf_kxml_kxml2': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/stefanhaustein/kxml2/master/license.txt",
            licenseName: "MIT"),
        'org_checkerframework_checker_compat_qual': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt",
            licenseName: "GPL v2 with the classpath exception"),
        'org_checkerframework_checker_qual': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt",
            licenseName: "GPL v2 with the classpath exception"),
        'org_checkerframework_dataflow': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt",
            licenseName: "GPL v2 with the classpath exception"),
        'org_checkerframework_dataflow_shaded': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt",
            licenseName: "GPL v2 with the classpath exception"),
        'org_checkerframework_javacutil': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt",
            licenseName: "GPL v2 with the classpath exception"),
        'org_ow2_asm_asm': new PropertyOverride(
            licenseUrl: "https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt"),
        'org_ow2_asm_asm_analysis': new PropertyOverride(
            licenseUrl: "https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt"),
        'org_ow2_asm_asm_commons': new PropertyOverride(
            licenseUrl: "https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt"),
        'org_ow2_asm_asm_tree': new PropertyOverride(
            licenseUrl: "https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt"),
        'org_ow2_asm_asm_util': new PropertyOverride(
            licenseUrl: "https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt"),
        'org_pcollections_pcollections': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/hrldcpr/pcollections/master/LICENSE"),
        'org_plumelib_plume_util': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/plume-lib/plume-util/master/LICENSE"),
        'org_plumelib_require_javadoc': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/plume-lib/require-javadoc/master/LICENSE"),
        'org_plumelib_reflection_util': new PropertyOverride(
            licenseUrl: "https://raw.githubusercontent.com/plume-lib/reflection-util/master/LICENSE"),
        'org_robolectric_annotations': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_junit': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_pluginapi': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_plugins_maven_dependency_resolver': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_resources': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_robolectric': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_sandbox': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_shadowapi': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_shadows_framework': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_shadows_multidex': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT",
            cipdSuffix: "cr1"),
        'org_robolectric_shadows_playservices': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_utils': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
        'org_robolectric_utils_reflector': new PropertyOverride(
            licensePath: "licenses/Codehaus_License-2009.txt",
            licenseName: "MIT"),
    ]

    Project project
    boolean skipLicenses

    void collectDependencies() {
        def compileConfig = project.configurations.getByName('compile').resolvedConfiguration
        def buildCompileConfig = project.configurations.getByName('buildCompile').resolvedConfiguration
        def testCompileConfig = project.configurations.getByName('testCompile').resolvedConfiguration
        def androidTestCompileConfig = project.configurations.getByName(
            'androidTestCompile').resolvedConfiguration
        List<String> topLevelIds = []
        Set<ResolvedConfiguration> deps = []
        deps += compileConfig.firstLevelModuleDependencies
        deps += buildCompileConfig.firstLevelModuleDependencies
        deps += testCompileConfig.firstLevelModuleDependencies
        deps += androidTestCompileConfig.firstLevelModuleDependencies

        deps.each { dependency ->
            topLevelIds.add(makeModuleId(dependency.module))
            collectDependenciesInternal(dependency)
        }

        topLevelIds.each { id -> dependencies.get(id).visible = true }

        testCompileConfig.resolvedArtifacts.each { artifact ->
            def id = makeModuleId(artifact)
            def dep = dependencies.get(id)
            assert dep != null : "No dependency collected for artifact ${artifact.name}"
            dep.testOnly = true
        }

        androidTestCompileConfig.resolvedArtifacts.each { artifact ->
            def dep = dependencies.get(makeModuleId(artifact))
            assert dep != null : "No dependency collected for artifact ${artifact.name}"
            dep.supportsAndroid = true
            dep.testOnly = true
        }

        buildCompileConfig.resolvedArtifacts.each { artifact ->
            def id = makeModuleId(artifact)
            def dep = dependencies.get(id)
            assert dep != null : "No dependency collected for artifact ${artifact.name}"
            dep.testOnly = false
        }

        compileConfig.resolvedArtifacts.each { artifact ->
            def id = makeModuleId(artifact)
            def dep = dependencies.get(id)
            assert dep != null : "No dependency collected for artifact ${artifact.name}"
            dep.supportsAndroid = true
            dep.testOnly = false
            dep.isShipped = true
        }

        PROPERTY_OVERRIDES.each { id, fallbackProperties ->
            if (fallbackProperties?.isShipped != null) {
                def dep = dependencies.get(id)
                if (dep != null) {
                    dep.isShipped = fallbackProperties.isShipped
                } else {
                    project.logger.warn("PROPERTY_OVERRIDES has stale dep: " + id)
                }
            }
        }
    }

    private ResolvedArtifactResult getPomFromArtifact(ComponentIdentifier componentId) {
        def component = project.dependencies.createArtifactResolutionQuery()
                .forComponents(componentId)
                .withArtifacts(MavenModule, MavenPomArtifact)
                .execute()
                .resolvedComponents[0]
        return component.getArtifacts(MavenPomArtifact)[0]
    }

    private void collectDependenciesInternal(ResolvedDependency dependency) {
        def id = makeModuleId(dependency.module)
        if (dependencies.containsKey(id)) {
            if (id in LOWER_VERSION_OVERRIDE &&
                   dependencies.get(id).version <= dependency.module.id.version) {
                return
            }
            // Use largest version for version conflict resolution. See crbug.com/1040958
            // https://docs.gradle.org/current/userguide/dependency_resolution.html#sec:version-conflict
            if (dependencies.get(id).version >= dependency.module.id.version) return
        }

        def childModules = []
        dependency.children.each { childDependency ->
            childModules += makeModuleId(childDependency.module)
        }

        if (dependency.getModuleArtifacts().size() != 1) {
            throw new IllegalStateException("The dependency ${id} does not have exactly one " +
                                            "artifact: ${dependency.getModuleArtifacts()}")
        }
        def artifact = dependency.getModuleArtifacts()[0]
        if (artifact.extension != 'jar' && artifact.extension != 'aar') {
            throw new IllegalStateException("Type ${artifact.extension} of ${id} not supported.")
        }

        dependencies.put(id, buildDepDescription(id, dependency, artifact, childModules))
        dependency.children.each {
            childDependency -> collectDependenciesInternal(childDependency)
        }
    }

    static String makeModuleId(ResolvedModuleVersion module) {
        // Does not include version because by default the resolution strategy for gradle is to use
        // the newest version among the required ones. We want to be able to match it in the
        // BUILD.gn file.
        def moduleId = sanitize("${module.id.group}_${module.id.name}")

        // Add 'android' suffix for guava-android so that its module name is distinct from the
        // module for guava.
        if (module.id.name == "guava" && module.id.version.contains("android")) {
            moduleId += "_android"
        }
        return moduleId
    }

    static String makeModuleId(ResolvedArtifact artifact) {
        // Does not include version because by default the resolution strategy for gradle is to use
        // the newest version among the required ones. We want to be able to match it in the
        // BUILD.gn file.
        def componentId = artifact.id.componentIdentifier
        def moduleId = sanitize("${componentId.group}_${componentId.module}")

        // Add 'android' suffix for guava-android so that its module name is distinct from the
        // module for guava.
        if (componentId.module == "guava" && componentId.version.contains("android")) {
            moduleId += "_android"
        }
        return moduleId
    }

    private static String sanitize(String input) {
        return input.replaceAll("[:.-]", "_")
    }

    private buildDepDescription(String id, ResolvedDependency dependency, ResolvedArtifact artifact,
                                List<String> childModules) {
        def pom = getPomFromArtifact(artifact.id.componentIdentifier).file
        def pomContent = new XmlSlurper(false, false).parse(pom)
        String licenseName = ''
        String licenseUrl = ''
        if (!skipLicenses) {
            (licenseName, licenseUrl) = resolveLicenseInformation(id, pomContent)
        }

        // Get rid of irrelevant indent that might be present in the XML file.
        def description = pomContent.description?.text()?.trim()?.replaceAll(/\s+/, " ")
        def displayName = pomContent.name?.text()
        if (!displayName) {
            displayName = dependency.module.id.name
        }

        return customizeDep(new DependencyDescription(
                id: id,
                artifact: artifact,
                group: dependency.module.id.group,
                name: dependency.module.id.name,
                version: dependency.module.id.version,
                extension: artifact.extension,
                componentId: artifact.id.componentIdentifier,
                children: Collections.unmodifiableList(new ArrayList<>(childModules)),
                licenseName: licenseName,
                licenseUrl: licenseUrl,
                licensePath: "",
                fileName: artifact.file.name,
                description: description,
                url: pomContent.url?.text(),
                displayName: displayName,
                exclude: false,
                cipdSuffix: "cr0",
        ))
    }

    private customizeDep(DependencyDescription dep) {
        if (dep.id?.startsWith("com_google_android_")) {
            project.logger.debug("Using Android license for ${dep.id}")
            dep.licenseUrl = ""
            // This should match fetch_all._ANDROID_SDK_LICENSE_PATH.
            dep.licensePath = "licenses/Android_SDK_License-December_9_2016.txt"
            if (dep.url?.isEmpty()) {
                dep.url = "https://developers.google.com/android/guides/setup"
            }
        } else if (dep.licenseUrl?.equals("http://openjdk.java.net/legal/gplv2+ce.html")) {
            project.logger.debug("Detected GPL v2 /w classpath license for ${dep.id}")
            // This avoids using html in a LICENSE file.
            dep.licenseUrl = ""
            dep.licenseName = "GPL v2 with the classpath exception"
            dep.licensePath = "licenses/GNU_v2_with_Classpath_Exception_1991.txt"
        }

        def fallbackProperties = PROPERTY_OVERRIDES.get(dep.id)
        if (fallbackProperties != null) {
            project.logger.debug("Using fallback properties for ${dep.id}")
            if (fallbackProperties.licenseName != null) {
              dep.licenseName = fallbackProperties.licenseName
            }
            if (fallbackProperties.licenseUrl != null) {
              dep.licenseUrl = fallbackProperties.licenseUrl
            }
            if (fallbackProperties.licensePath != null) {
                dep.licensePath = fallbackProperties.licensePath
            }
            if (fallbackProperties.url != null) {
                dep.url = fallbackProperties.url
            }
            if (fallbackProperties.cipdSuffix != null) {
              dep.cipdSuffix = fallbackProperties.cipdSuffix
            }
            if (fallbackProperties.generateTarget != null) {
              dep.generateTarget = fallbackProperties.generateTarget
            }
            if (fallbackProperties.exclude != null) {
              dep.exclude = fallbackProperties.exclude
            }
        }

        if (skipLicenses) {
            dep.licenseName = ''
            dep.licensePath = ''
            dep.licenseUrl = ''
            if (dep.id?.endsWith('license')) {
                dep.exclude = true
            }
        }

        return dep
    }

    private resolveLicenseInformation(String id, GPathResult pomContent) {
      GPathResult licenses = pomContent?.licenses?.license
      if (!licenses || licenses.size() == 0) {
          return ["License Missing Error", ""]
      } else if (licenses.size() > 1) {
          def allUrls = ''
          for (def license : licenses) {
              allUrls += license.url.text() + " "
          }
          return ["Multiple Licenses Error: ${allUrls}", ""]
      }

      return [licenses[0].name.text(), licenses[0].url.text()]
    }

    static class DependencyDescription {
        String id
        ResolvedArtifact artifact
        String group, name, version, extension, displayName, description, url
        String licenseName, licenseUrl, licensePath
        String fileName
        boolean supportsAndroid, visible, exclude, testOnly, isShipped
        boolean generateTarget = true
        boolean licenseAndroidCompatible
        ComponentIdentifier componentId
        List<String> children
        String cipdSuffix
    }

    static class PropertyOverride {
      String description
      String url
      String licenseName, licenseUrl, licensePath
      String cipdSuffix
      Boolean isShipped
      // Set to true if this dependency is not needed.
      Boolean exclude
      // Set to false to skip creation of BUILD.gn target.
      Boolean generateTarget
    }
}
