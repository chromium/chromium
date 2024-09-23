// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import groovy.transform.AutoClone
import groovy.util.slurpersupport.GPathResult
import org.gradle.api.Project
import org.gradle.api.artifacts.repositories.ArtifactRepository
import org.gradle.api.artifacts.ResolvedArtifact
import org.gradle.api.artifacts.ResolvedConfiguration
import org.gradle.api.artifacts.ResolvedDependency
import org.gradle.api.artifacts.ResolvedModuleVersion
import org.gradle.api.artifacts.component.ComponentIdentifier
import org.gradle.api.logging.Logger

/**
 * Parses the project dependencies and generates a graph of {@link ChromiumDepGraph.DependencyDescription} objects to
 * make the data manipulation easier.
 */
class ChromiumDepGraph {

    private static final String DEFAULT_CIPD_SUFFIX = 'cr1'

    // Some libraries don't properly fill their POM with the appropriate licensing information. It is provided here from
    // manual lookups. Note that licenseUrl must provide textual content rather than be an html page.
    static final Map<String, PropertyOverride> PROPERTY_OVERRIDES = [
        androidx_multidex_multidex: new PropertyOverride(
            url: 'https://maven.google.com/androidx/multidex/multidex/2.0.0/multidex-2.0.0.aar'),
        com_google_android_datatransport_transport_api: new PropertyOverride(
            description: 'Interfaces for data logging in GmsCore SDKs.'),
        com_google_android_datatransport_transport_backend_cct: new PropertyOverride(
            exclude: true),  // We're not using datatransport functionality.
        com_google_android_datatransport_transport_runtime: new PropertyOverride(
            exclude: true),  // We're not using datatransport functionality.
        com_google_android_gms_play_services_cloud_messaging: new PropertyOverride(
            description: 'Firebase Cloud Messaging library that interfaces with GmsCore.'),
        com_google_android_gms_play_services_base: new PropertyOverride(
            description: 'Base library for gmscore / Google Play Services.'),
        com_google_android_gms_play_services_location: new PropertyOverride(
            description: 'Provides data about the device\'s physical location via gmscore.'),
        com_google_auto_service_auto_service_annotations: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        com_google_auto_value_auto_value_annotations: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        com_google_code_gson_gson: new PropertyOverride(
            url: 'https://github.com/google/gson',
            description: 'A Java serialization/deserialization library to convert Java Objects into JSON and back',
            licenseUrl: 'https://raw.githubusercontent.com/google/gson/master/LICENSE',
            licenseName: 'Apache 2.0'),
        com_google_errorprone_error_prone_annotation: new PropertyOverride(
            // Robolectric has a (seemingly unnecessary) dep on this. It's meant to be needed
            // only for writing custom Error Prone checks. Chrome's copy is within the
            // Error Prone fat jar: //third_party/android_build_tools/error_prone
            // Depending on this fat jar pulls in a conflicting copy of protobuf library.
            exclude: true),
        com_google_errorprone_error_prone_annotations: new PropertyOverride(
            url: 'https://github.com/google/error-prone/tree/master/annotations',
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0',
            description: 'ErrorProne Annotations.',),
        com_google_firebase_firebase_annotations: new PropertyOverride(
            description: 'Common annotations for Firebase SKDs.'),
        com_google_firebase_firebase_common: new PropertyOverride(
            description: 'Common classes for Firebase SDKs.'),
        com_google_firebase_firebase_components: new PropertyOverride(
            description: 'Provides dependency management for Firebase SDKs.'),
        com_google_firebase_firebase_datatransport: new PropertyOverride(
            exclude: true),  // We're not using datatransport functionality.
        com_google_firebase_firebase_encoders_json: new PropertyOverride(
            description: 'JSON encoders used in Firebase SDKs.'),
        com_google_firebase_firebase_encoders: new PropertyOverride(
            description: 'Commonly used encoders for Firebase SKDs.'),
        com_google_firebase_firebase_iid_interop: new PropertyOverride(
            description: 'Interface library for Firebase IID SDK.'),
        com_google_firebase_firebase_iid: new PropertyOverride(
            description: 'Firebase IID SDK to get access to Instance IDs.'),
        com_google_firebase_firebase_installations_interop: new PropertyOverride(
            description: 'Interface library for Firebase Installations SDK.'),
        com_google_firebase_firebase_installations: new PropertyOverride(
            description: 'Firebase Installations SDK containing the client libraries to manage FIS.'),
        com_google_firebase_firebase_measurement_connector: new PropertyOverride(
            description: 'Bridge interfaces for Firebase analytics into GmsCore.'),
        com_google_firebase_firebase_messaging: new PropertyOverride(
            description: 'Firebase Cloud Messaging SDK to send and receive push messages via FCM.'),
        com_google_googlejavaformat_google_java_format: new PropertyOverride(
            url: 'https://github.com/google/google-java-format',
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        com_google_guava_failureaccess: new PropertyOverride(
            url: 'https://github.com/google/guava',
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        com_google_guava_guava: new PropertyOverride(
            url: 'https://github.com/google/guava',
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0',
            // Both -jre and -android versions are listed. Filter to only the -jre ones.
            versionFilter: '-jre'),
        com_google_guava_guava_android: new PropertyOverride(
            url: 'https://github.com/google/guava',
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0',
            // Both -jre and -android versions are listed. Filter to only the -android ones.
            versionFilter: '-android'),
        com_squareup_wire_wire_runtime_jvm: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        org_bouncycastle_bcprov_jdk18on: new PropertyOverride(
            cpePrefix: 'cpe:/a:bouncycastle:legion-of-the-bouncy-castle:1.72',
            url: 'https://github.com/bcgit/bc-java',
            licensePath: 'licenses/Bouncy_Castle-2015.txt',
            licenseName: 'MIT'),
        org_codehaus_mojo_animal_sniffer_annotations: new PropertyOverride(
            url: 'http://www.mojohaus.org/animal-sniffer/animal-sniffer-annotations/',
            description: 'Animal Sniffer Annotations allow marking methods which Animal Sniffer should ignore ' +
                         'signature violations of.',
            /* groovylint-disable-next-line LineLength */
            licenseUrl: 'https://raw.githubusercontent.com/mojohaus/animal-sniffer/master/animal-sniffer-annotations/pom.xml',
            licensePath: 'licenses/Codehaus_License-2009.txt',
            licenseName: 'MIT'),
        com_google_protobuf_protobuf_lite: new PropertyOverride(
            exclude: true, // There is a phantom dep on this target, but this is deprecated and not used in chrome.
            url: 'https://github.com/protocolbuffers/protobuf/blob/master/java/README.md',
            licenseUrl: 'https://raw.githubusercontent.com/protocolbuffers/protobuf/master/LICENSE',
            licenseName: 'BSD'),
        com_google_protobuf_protobuf_javalite: new PropertyOverride(
            url: 'https://github.com/protocolbuffers/protobuf/blob/master/java/lite.md',
            licenseUrl: 'https://raw.githubusercontent.com/protocolbuffers/protobuf/master/LICENSE',
            licenseName: 'BSD'),
        jakarta_inject_jakarta_inject_api: new PropertyOverride(
            // Help gradle resolve the same version that our 3pp script does.
            versionFilter: '\\d+\\.\\d+\\.\\d+$'),
        javax_annotation_javax_annotation_api: new PropertyOverride(
            isShipped: false,  // Annotations are stripped by R8.
            licenseName: 'CDDLv1.1',
            licensePath: 'licenses/CDDLv1.1.txt'),
        javax_annotation_jsr250_api: new PropertyOverride(
            isShipped: false,  // Annotations are stripped by R8.
            licenseName: 'CDDLv1.0',
            licensePath: 'licenses/CDDLv1.0.txt'),
        net_bytebuddy_byte_buddy: new PropertyOverride(
            url: 'https://github.com/raphw/byte-buddy',
            licenseUrl: 'https://raw.githubusercontent.com/raphw/byte-buddy/master/LICENSE',
            licenseName: 'Apache 2.0'),
        net_bytebuddy_byte_buddy_agent: new PropertyOverride(
            url: 'https://github.com/raphw/byte-buddy',
            licenseUrl: 'https://raw.githubusercontent.com/raphw/byte-buddy/master/LICENSE',
            licenseName: 'Apache 2.0'),
        net_bytebuddy_byte_buddy_android: new PropertyOverride(
            url: 'https://github.com/raphw/byte-buddy',
            licenseUrl: 'https://raw.githubusercontent.com/raphw/byte-buddy/master/LICENSE',
            licenseName: 'Apache 2.0'),
        org_checkerframework_checker_compat_qual: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt',
            licenseName: 'GPL v2 with the classpath exception'),
        org_checkerframework_checker_qual: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt',
            licenseName: 'GPL v2 with the classpath exception'),
        org_checkerframework_checker_util: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/typetools/checker-framework/master/checker-util/LICENSE.txt',
            licenseName: 'MIT'),
        org_conscrypt_conscrypt_openjdk_uber: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/google/conscrypt/master/LICENSE',
            licenseName: 'Apache 2.0'),
        org_hamcrest_hamcrest: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/hamcrest/JavaHamcrest/master/LICENSE',
            licenseName: 'BSD'),
        org_jsoup_jsoup: new PropertyOverride(
            cpePrefix: 'cpe:/a:jsoup:jsoup:1.14.3',
            licenseUrl: 'https://raw.githubusercontent.com/jhy/jsoup/master/LICENSE',
            licenseName: 'The MIT License'),
        org_mockito_mockito_android: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/mockito/mockito/main/LICENSE',
            licenseName: 'The MIT License'),
        org_mockito_mockito_core: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/mockito/mockito/main/LICENSE',
            licenseName: 'The MIT License'),
        org_mockito_mockito_subclass: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/mockito/mockito/main/LICENSE',
            licenseName: 'The MIT License'),
        org_objenesis_objenesis: new PropertyOverride(
            url: 'http://objenesis.org/index.html',
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        org_ow2_asm_asm: new PropertyOverride(
            licenseUrl: 'https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt',
            licenseName: 'BSD'),
        org_ow2_asm_asm_analysis: new PropertyOverride(
            licenseUrl: 'https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt',
            licenseName: 'BSD'),
        org_ow2_asm_asm_commons: new PropertyOverride(
            licenseUrl: 'https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt',
            licenseName: 'BSD'),
        org_ow2_asm_asm_tree: new PropertyOverride(
            licenseUrl: 'https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt',
            licenseName: 'BSD'),
        org_ow2_asm_asm_util: new PropertyOverride(
            licenseUrl: 'https://gitlab.ow2.org/asm/asm/raw/master/LICENSE.txt',
            licenseName: 'BSD'),
        org_robolectric_annotations: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_junit: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_nativeruntime: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_nativeruntime_dist_compat: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_pluginapi: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_plugins_maven_dependency_resolver: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_resources: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_robolectric: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_sandbox: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_shadowapi: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_shadows_framework: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_shadows_versioning: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_utils: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        org_robolectric_utils_reflector: new PropertyOverride(
            licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE',
            licenseName: 'MIT'),
        // Prevent version changing ~weekly. https://crbug.com/1257197
        org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm: new PropertyOverride(
            resolveVersion: '1.8.1'),
        org_jetbrains_kotlinx_kotlinx_coroutines_android: new PropertyOverride(
            resolveVersion: '1.8.1'),
        org_jetbrains_kotlinx_kotlinx_coroutines_guava: new PropertyOverride(
            resolveVersion: '1.8.1'),
        org_jetbrains_kotlinx_kotlinx_serialization_core_jvm: new PropertyOverride(
            resolveVersion: '1.7.2'),
        io_grpc_grpc_binder: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        io_grpc_grpc_core: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        io_grpc_grpc_api: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        io_grpc_grpc_context: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        io_grpc_grpc_protobuf_lite: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        io_grpc_grpc_stub: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
        io_perfmark_perfmark_api: new PropertyOverride(
            licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
            licenseName: 'Apache 2.0'),
    ]

    // Bill of materials (BOM) deps are used to specify versions for other dependencies and don't have children or
    // artifacts of their own. Add other such empty deps here when we encounter them.
    private static final Set<String> ALLOWED_EMPTY_DEPS = [] as Set

    // Local text versions of HTML licenses. This cannot replace PROPERTY_OVERRIDES because some libraries refer to
    // license templates such as https://opensource.org/licenses/MIT.
    // Keys should be 'https', since customizeLicenses() will normalize URLs to https.
    static final Map<String, String> LICENSE_OVERRIDES = [
      'https://developer.android.com/studio/terms.html': 'licenses/Android_SDK_License-December_9_2016.txt',
      'https://openjdk.java.net/legal/gplv2+ce.html': 'licenses/GNU_v2_with_Classpath_Exception_1991.txt',
      'https://scripts.sil.org/cms/scripts/page.php?item_id=OFL_web': 'licenses/SIL_Open_Font.txt',
      'https://www.unicode.org/copyright.html#License': 'licenses/Unicode.txt',
      'https://www.unicode.org/license.html': 'licenses/Unicode.txt',
    ]

    final Map<String, DependencyDescription> dependencies = [:]
    Project[] projects
    Logger logger
    boolean skipLicenses

    static String makeModuleId(ResolvedModuleVersion module) {
        // Does not include version because by default the resolution strategy for gradle is to use the newest version
        // among the required ones. We want to be able to match it in the BUILD.gn file.
        String moduleId = sanitize("${module.id.group}_${module.id.name}")

        // Add 'android' suffix for guava-android so that its module name is distinct from the module for guava.
        if (module.id.name == 'guava' && module.id.version.contains('android')) {
            moduleId += '_android'
        }
        return moduleId
    }

    static String makeModuleId(ResolvedArtifact artifact) {
        // Does not include version because by default the resolution strategy for gradle is to use the newest version
        // among the required ones. We want to be able to match it in the BUILD.gn file.
        ComponentIdentifier componentId = artifact.id.componentIdentifier
        String moduleId = sanitize("${componentId.group}_${componentId.module}")

        // Add 'android' suffix for guava-android so that its module name is distinct from the module for guava.
        if (componentId.module == 'guava' && componentId.version.contains('android')) {
            moduleId += '_android'
        }
        return moduleId
    }

    void collectDependencies() {
        Set<ResolvedConfiguration> deps = [] as Set
        Map<String, List<ResolvedArtifact>> resolvedArtifacts = [:]
        String[] configNames = [
            'compile',
            'buildCompile',
            'testCompile',
            'androidTestCompile',
            'buildCompileNoDeps'
        ]
        for (Project project : projects) {
            for (String configName : configNames) {
                ResolvedConfiguration resolvedConfig = project.configurations.getByName(configName).resolvedConfiguration
                deps += resolvedConfig.firstLevelModuleDependencies
                if (!resolvedArtifacts.containsKey(configName)) {
                    resolvedArtifacts[configName] = []
                }
                resolvedArtifacts[configName].addAll(resolvedConfig.resolvedArtifacts)
            }
        }

        List<String> topLevelIds = []
        deps.each { dependency ->
            topLevelIds.add(makeModuleId(dependency.module))
            collectDependenciesInternal(dependency)
        }

        topLevelIds.each { id -> dependencies.get(id).visible = true }

        resolvedArtifacts['testCompile'].each { artifact ->
            String id = makeModuleId(artifact)
            DependencyDescription dep = dependencies.get(id)
            assert dep : "No dependency collected for artifact ${artifact.name}"
            dep.testOnly = true
        }

        resolvedArtifacts['androidTestCompile'].each { artifact ->
            DependencyDescription dep = dependencies.get(makeModuleId(artifact))
            assert dep : "No dependency collected for artifact ${artifact.name} (${makeModuleId(artifact)})"
            dep.supportsAndroid = true
            dep.testOnly = true
        }

        resolvedArtifacts['buildCompile'].each { artifact ->
            String id = makeModuleId(artifact)
            DependencyDescription dep = dependencies.get(id)
            assert dep : "No dependency collected for artifact ${artifact.name}"
            dep.usedInBuild = true
            dep.testOnly = false
        }

        resolvedArtifacts['buildCompileNoDeps'].each { artifact ->
            String id = makeModuleId(artifact)
            DependencyDescription dep = dependencies.get(id)
            assert dep : "No dependency collected for artifact ${artifact.name}"
            dep.usedInBuild = true
            dep.testOnly = false
        }

        List<ResolvedArtifact> compileResolvedArtifacts = resolvedArtifacts['compile']
        compileResolvedArtifacts.each { artifact ->
            String id = makeModuleId(artifact)
            DependencyDescription dep = dependencies.get(id)
            assert dep : "No dependency collected for artifact ${artifact.name}"
            dep.supportsAndroid = true
            dep.testOnly = false
            dep.isShipped = true
        }

        PROPERTY_OVERRIDES.each { id, overrides ->
            DependencyDescription dep = dependencies.get(id)
            if (dep) {
                // Null-check is required since isShipped is a boolean. This
                // check must come after all the deps are resolved instead of in
                // customizeDep, since otherwise it gets overwritten.
                if (overrides?.isShipped != null) {
                    dep.isShipped = overrides.isShipped
                }
                // if overrideLatest is true, set it recursively on the dep and
                // all its children. This makes it easier to manage since you do
                // not have to set it on a whole set of old deps.
                if (overrides?.overrideLatest) {
                    recursivelyOverrideLatestVersion(dep)
                }
                dep.versionFilter = overrides.versionFilter
            } else {
                logger.warn('PROPERTY_OVERRIDES has stale dep: ' + id)
            }
        }
    }

    private static String sanitize(String input) {
        return input.replaceAll('[:.-]', '_')
    }

    private void recursivelyOverrideLatestVersion(DependencyDescription dep) {
        dep.overrideLatest = true
        dep.children.each { childID ->
            PropertyOverride overrides = PROPERTY_OVERRIDES.get(childID)
            if (!overrides?.resolveVersion) {
                DependencyDescription child = dependencies.get(childID)
                recursivelyOverrideLatestVersion(child)
            }
        }
    }

    private void collectDependenciesInternal(ResolvedDependency dependency) {
        String id = makeModuleId(dependency.module)
        if (dependencies.containsKey(id)) {
            String gotVersion = dependency.module.id.version
            if (dependencies.get(id).version == gotVersion) {
                return
            }
            PropertyOverride overrides = PROPERTY_OVERRIDES.get(id)
            if (overrides?.resolveVersion) {
                if (overrides.resolveVersion != gotVersion) {
                    return
                }
            } else if (isVersionLower(gotVersion, dependencies.get(id).version)) {
                // Default to using largest version for version conflict resolution. See http://crbug.com/1040958.
                // https://docs.gradle.org/current/userguide/dependency_resolution.html#sec:version-conflict
                return
            }
        }

        List<ResolvedDependency> childDependenciesWithArtifacts = []
        List<String> childModules = []
        dependency.children.each { childDependency ->
            // Replace dependency which acts as a redirect (ex: org.jetbrains.kotlinx:kotlinx-coroutines-core) with
            // dependencies it redirects to.
            if (childDependency.moduleArtifacts) {
                childDependenciesWithArtifacts += childDependency
            } else {
                if (childDependency.children) {
                    childDependenciesWithArtifacts += childDependency.children
                } else {
                    String childDepId = makeModuleId(childDependency.module)
                    if (!childDepId.endsWith("_bom") && childDepId !in ALLOWED_EMPTY_DEPS) {
                        // BOM dependencies are deps that only specify other deps as dependencies but have no
                        // artifact of their own. These typically have _bom at the end of their names but may also
                        // be identified by looking at their pom.xml file. For more context see maven's doc:
                        /* groovylint-disable-next-line LineLength */
                        // https://maven.apache.org/guides/introduction/introduction-to-dependency-mechanism.html#bill-of-materials-bom-poms
                        throw new IllegalStateException(
                                "The dependency ${childDepId} has no children and no artifacts. If this is " +
                                'expected (e.g. for BOM dependencies), then please add it to the ' +
                                '|ALLOWED_EMPTY_DEPS| set.')
                    }
                }
            }
        }

        childDependenciesWithArtifacts.each { childDependency ->
            childModules += makeModuleId(childDependency.module)
        }

        if (dependency.moduleArtifacts.empty) {
            assert childModules : "${id} has no children and no artifacts."
            dependencies.put(id, buildDepDescriptionNoArtifact(id, dependency, childModules))
            childDependenciesWithArtifacts.each {
                childDependency -> collectDependenciesInternal(childDependency)
            }
        } else if (!areAllModuleArtifactsSameFile(dependency.moduleArtifacts)) {
            throw new IllegalStateException("The dependency ${id} has multiple different artifacts: " +
                                            "${dependency.moduleArtifacts}")
        } else {
            ResolvedArtifact artifact = dependency.moduleArtifacts[0]
            if (artifact.extension != 'jar' && artifact.extension != 'aar') {
                throw new IllegalStateException("Type ${artifact.extension} of ${id} not supported.")
            }
            dependencies.put(id, buildDepDescription(id, dependency, artifact, childModules))
            childDependenciesWithArtifacts.each {
                childDependency -> collectDependenciesInternal(childDependency)
            }
        }
    }

    private boolean areAllModuleArtifactsSameFile(Set<ResolvedArtifact> artifacts) {
        String expectedPath
        for (ResolvedArtifact artifact : artifacts) {
            String path = artifact.file.absolutePath
            if (expectedPath == null) {
                expectedPath = path
                continue
            }
            if (expectedPath != path) {
                return false
            }
        }
        return true
    }

    private DependencyDescription buildDepDescriptionNoArtifact(
            String id, ResolvedDependency dependency, List<String> childModules) {

        return customizeDep(new DependencyDescription(
                id: id,
                group: dependency.module.id.group,
                name: dependency.module.id.name,
                version: dependency.module.id.version,
                extension: 'group',
                children: Collections.unmodifiableList(new ArrayList<>(childModules)),
                directoryName: id.toLowerCase(),
                displayName: dependency.module.id.name,
                exclude: false,
                cipdSuffix: DEFAULT_CIPD_SUFFIX,
        ))
    }

    private DependencyDescription buildDepDescription(
            String id, ResolvedDependency dependency, ResolvedArtifact artifact, List<String> childModules) {
        String pomUrl, repoUrl
        GPathResult pomContent
        (repoUrl, pomUrl, pomContent) = computePomFromArtifact(artifact)

        List<LicenseSpec> licenses = []
        if (!skipLicenses) {
            licenses = resolveLicenseInformation(pomContent)
        }

        // Build |fileUrl| by swapping '.pom' file extension with artifact file extension.
        String fileUrl = pomUrl[0..-4] + artifact.extension
        // Check that the URL is correct explicitly here. Otherwise, we won't
        // find out until 3pp bot runs.
        checkDownloadable(fileUrl)

        // Get rid of irrelevant indent that might be present in the XML file.
        String description = pomContent.description?.text()?.trim()?.replaceAll(/\s+/, ' ')
        String displayName = pomContent.name?.text()
        displayName = displayName ?: dependency.module.id.name

        return customizeDep(new DependencyDescription(
                id: id,
                artifact: artifact,
                group: dependency.module.id.group,
                name: dependency.module.id.name,
                version: dependency.module.id.version,
                extension: artifact.extension,
                componentId: artifact.id.componentIdentifier,
                children: Collections.unmodifiableList(new ArrayList<>(childModules)),
                licenses: licenses,
                directoryName: id.toLowerCase(),
                fileName: artifact.file.name,
                fileUrl: fileUrl,
                repoUrl: repoUrl,
                description: description,
                url: pomContent.url?.text(),
                displayName: displayName,
                exclude: false,
                cipdSuffix: DEFAULT_CIPD_SUFFIX,
        ))
    }

    private void customizeLicenses(DependencyDescription dep, PropertyOverride overrides) {
        for (LicenseSpec license : dep.licenses) {
            if (!license.url) {
                continue
            }
            String normalizedLicenseUrl = license.url.replace('http://', 'https://')
            String licenseOverridePath = LICENSE_OVERRIDES[normalizedLicenseUrl]
            if (licenseOverridePath) {
                license.url = ''
                license.path = licenseOverridePath
            }
        }

        if (dep.id?.startsWith('com_google_android_')) {
            logger.debug("Using Android license for $dep.id")
            dep.licenses.clear()
            dep.licenses.add(new LicenseSpec(
                name: 'Android Software Development Kit License',
                path: 'licenses/Android_SDK_License-December_9_2016.txt'))
        }

        if (overrides) {
            if (overrides.licenseName) {
                dep.licenses.clear()
                LicenseSpec license = new LicenseSpec(
                    name : overrides.licenseName,
                    path: overrides.licensePath,
                    url: overrides.licenseUrl,
                )
                dep.licenses.add(license)
            } else {
                if (overrides.licensePath || overrides.licenseUrl) {
                    throw new IllegalStateException('PropertyOverride must specify "licenseName" if either ' +
                                                    '"licensePath" or "licenseUrl" is specified.')
                }
            }
        }
    }

    private DependencyDescription customizeDep(DependencyDescription dep) {
        if (dep.id?.startsWith('com_google_android_')) {
            // Many google dependencies don't set their URL, here is a good default.
            dep.url = dep.url ?: 'https://developers.google.com/android/guides/setup'
        } else if (dep.id?.startsWith('com_google_firebase_')) {
            // Same as above for some firebase dependencies.
            dep.url = dep.url ?: 'https://firebase.google.com'
        }

        PropertyOverride overrides = PROPERTY_OVERRIDES.get(dep.id)
        if (overrides) {
            logger.debug("Using override properties for $dep.id")
            dep.with {
                description = overrides.description ?: description
                url = overrides.url ?: url
                cipdSuffix = overrides.cipdSuffix ?: cipdSuffix
                cpePrefix = overrides.cpePrefix ?: cpePrefix
                // Boolean properties require explicit null checks instead of only when truish.
                if (overrides.generateTarget != null) {
                    generateTarget = overrides.generateTarget
                }
                if (overrides.exclude != null) {
                    exclude = overrides.exclude
                }
            }
        }

        if (skipLicenses) {
            dep.licenses = []
            if (dep.id?.endsWith('license')) {
                dep.exclude = true
            }
        } else {
            customizeLicenses(dep, overrides)
        }

        return dep
    }

    private List<LicenseSpec> resolveLicenseInformation(GPathResult pomContent) {
        GPathResult licenses = pomContent?.licenses?.license
        if (!licenses) {
            return []
        }

        List<LicenseSpec> out = []
        for (GPathResult license : licenses) {
            out.add(new LicenseSpec(
              name: license.name.text(),
              url: license.url.text()
          ))
        }
        return out
    }

    private List computePomFromArtifact(ResolvedArtifact artifact) {
        ComponentIdentifier component = artifact.id.componentIdentifier
        String componentPomSubpath = String.format('%s/%s/%s/%s-%s.pom',
                component.group.replace('.', '/'),
                component.module,
                component.version,
                component.module,
                // While mavenCentral and google use "version", https://androidx.dev uses "timestampedVersion" as part
                // of the file url
                component.hasProperty('timestampedVersion') ? component.timestampedVersion : component.version)
        List<String> repoUrls = []
        for (Project project : projects) {
            for (ArtifactRepository repository : project.repositories.asList()) {
                String repoUrl = repository.properties.get('url')
                // Some repo url may have trailing '/' and this breaks the file url generation below. So remove it if
                // present.
                if (repoUrl.endsWith('/')) {
                    repoUrl = repoUrl[0..-2]
                }
                // Deduplicate while collecting repo urls since subprojects (e.g. androidx) may use the same repos. Use
                // a list instead of a set to preserve order. Since there are very few repositories, 2-3 per project,
                // this O(n^2) complexity is acceptable.
                if (repoUrls.contains(repoUrl)) {
                    continue
                }
                // If the component is from androidx, we likely need the nightly builds from androidx.dev, so check that
                // repo first to avoid potential 404s. Inserting at the front preserves order between google and
                // mavenCentral.
                if (component.group.contains('androidx') && repoUrl.contains('androidx.dev')) {
                    repoUrls.add(0, repoUrl)
                } else {
                    repoUrls.add(repoUrl)
                }
            }
        }
        for (String repoUrl : repoUrls) {
            // Constructs the file url for pom. For example, with
            //   * repoUrl as "https://maven.google.com"
            //   * component.group as "android.arch.core"
            //   * component.module as "common"
            //   * component.version as "1.1.1"
            //
            // The file url will be: https://maven.google.com/android/arch/core/common/1.1.1/common-1.1.1.pom
            String fileUrl = String.format('%s/%s', repoUrl, componentPomSubpath)
            try {
                GPathResult content = new XmlSlurper(
                        false /* validating */, false /* namespaceAware */).parse(fileUrl)
                logger.debug("Succeeded in resolving url $fileUrl")
                return [repoUrl, fileUrl, content]
            } catch (any) {
                logger.debug("Failed in resolving url $fileUrl")
            }
        }
        throw new RuntimeException("Could not find pom from artifact $componentPomSubpath in $repoUrls")
    }

    private void checkDownloadable(String url) {
        // file: URLs happen when using fetch_all_androidx.py --local-repo.
        if (url.startsWith('file:')) {
            if (!new File(new URI(url).getPath()).exists()) {
                throw new RuntimeException('File not found: ' + url)
            }
            return
        }
        // Use a background thread to avoid slowing down main thread.
        // Saves about 80 seconds currently.
        new Thread().start(() -> {
            HttpURLConnection http = new URL(url).openConnection()
            http.requestMethod = 'HEAD'
            if (http.responseCode != 200) {
                /* groovylint-disable-next-line PrintStackTrace */
                new RuntimeException("Resolved POM but could not resolve $url").printStackTrace()
                // Exception is logged and ignored if thrown, so explicitly exit.
                /* groovylint-disable-next-line SystemExit */
                System.exit(1)
            }
            http.disconnect()
        });
    }

    // Checks if currentVersion is lower than versionInQuestion.
    private boolean isVersionLower(String currentVersion, String versionInQuestion) {
        List verA = currentVersion.tokenize('.')
        List verB = versionInQuestion.tokenize('.')
        int commonIndices = Math.min(verA.size(), verB.size())
        for (int i = 0; i < commonIndices; ++i) {
            // toInteger could fail as some versions are 2.11.alpha-06.
            // so revert to a string comparison.
            try {
                int numA = verA[i].toInteger()
                int numB = verB[i].toInteger()
                if (numA == numB) {
                    continue
                }
                return numA < numB
            } catch (any) {
                logger.debug('Using String comparison for a version check.')
                // This could lead to issues where a version such as 2.11.alpha11
                // is registered as less than 2.11.alpha9.
                return verA[i] < verB[i]
            }
        }

        // If we got this far then all the common indices are identical,
        // so whichever version is longer is larger.
        return verA.size() < verB.size()
    }

    @AutoClone
    static class DependencyDescription {

        String id
        ResolvedArtifact artifact
        String group, name, version, extension, displayName, description, url
        List<LicenseSpec> licenses
        String fileName, fileUrl
        // |repoUrl| is the url to the repo that hosts this dep's artifact
        // (|fileUrl|). Basically |fileurl|.startswith(|repoUrl|). |url| is the
        // project homepage as supplied by the developer.
        String repoUrl
        // The local directory name to store the files like artifact, license file, 3pp subdirectory, and etc. Must be
        // lowercase since 3pp uses the directory name as part of the CIPD names. However CIPD does not allow uppercase
        // in names.
        String directoryName
        boolean supportsAndroid, visible, exclude, testOnly, isShipped, usedInBuild
        boolean generateTarget = true
        boolean licenseAndroidCompatible
        ComponentIdentifier componentId
        List<String> children
        String cipdSuffix
        String cpePrefix
        // When set overrides the version downloaded by the 3pp fetch script to
        // be, instead of the latest available, the resolved version by gradle
        // in this run.
        Boolean overrideLatest
        // When set, consider only versions that contain this string.
        String versionFilter

    }

    static class LicenseSpec {

        String name, url, path

    }

    static class PropertyOverride {

        String description
        String url
        String licenseName, licenseUrl, licensePath
        String cipdSuffix
        String cpePrefix
        String resolveVersion
        Boolean isShipped
        // Set to true if this dependency is not needed.
        Boolean exclude
        // Set to false to skip creation of BUILD.gn target.
        Boolean generateTarget
        Boolean overrideLatest
        String versionFilter

    }

}
