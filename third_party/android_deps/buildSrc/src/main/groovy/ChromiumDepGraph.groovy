// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import groovy.transform.AutoClone
import groovy.util.slurpersupport.GPathResult
import org.gradle.api.Project
import org.gradle.api.artifacts.*
import org.gradle.api.artifacts.component.ComponentIdentifier
import org.gradle.api.artifacts.repositories.ArtifactRepository
import org.gradle.api.artifacts.result.*
import java.util.concurrent.*
import java.time.*
import org.gradle.api.logging.Logger
import java.nio.file.Path
import java.nio.file.Paths

/**
 * Parses the project dependencies and generates a graph of {@link ChromiumDepGraph.DependencyDescription} objects to
 * make the data manipulation easier.
 */
class ChromiumDepGraph {

    private static final String DEFAULT_CIPD_SUFFIX = 'cr2'

    // Some libraries don't properly fill their POM with the appropriate licensing information. It is provided here from
    // manual lookups. Note that licenseUrl must provide textual content rather than be an html page.
    static final Map<String, PropertyOverride> PROPERTY_OVERRIDES = [
            androidx_datastore_datastore_preferences_external_protobuf: new PropertyOverride(
                    licenseUrl: 'https://raw.githubusercontent.com/protocolbuffers/protobuf/refs/heads/main/LICENSE',
                    licenseName: 'BSD'),
            com_android_extensions_xr_extensions_xr: new PropertyOverride(
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache-2.0'),
            com_google_android_datatransport_transport_api: new PropertyOverride(
                    description: 'Interfaces for data logging in gmscore SDKs.'),
            com_google_android_gms_play_services_cloud_messaging: new PropertyOverride(
                    description: 'Firebase Cloud Messaging library that interfaces with gmscore.'),
            com_google_android_gms_play_services_location: new PropertyOverride(
                    description: 'Provides data about the device\'s physical location via gmscore.'),
            com_google_ar_impress: new PropertyOverride(
                    description: 'Impress shows GLTF models on XR devices, and provides advanced materials and rendering.\n'
                               + 'A dependency of https://developer.android.com/jetpack/androidx/releases/xr-scenecore.\n',
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache-2.0'),
            com_google_auto_service_auto_service_annotations: new PropertyOverride(
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache 2.0'),
            com_google_auto_value_auto_value_annotations: new PropertyOverride(
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache 2.0'),
            com_google_code_gson_gson: new PropertyOverride(
                    cpePrefix: 'cpe:/a:google:gson',
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
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache 2.0',
                    description: 'ErrorProne Annotations.',),
            com_google_firebase_firebase_annotations: new PropertyOverride(
                    description: 'Common annotations for Firebase SKDs.'),
            com_google_firebase_firebase_common: new PropertyOverride(
                    description: 'Common classes for Firebase SDKs.'),
            com_google_firebase_firebase_components: new PropertyOverride(
                    description: 'Provides dependency management for Firebase SDKs.'),
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
                    description: 'Bridge interfaces for Firebase analytics into gmscore.'),
            com_google_firebase_firebase_messaging: new PropertyOverride(
                    description: 'Firebase Cloud Messaging SDK to send and receive push messages via FCM.'),
            com_google_guava_failureaccess: new PropertyOverride(
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache 2.0'),
            com_google_guava_guava: new PropertyOverride(
                    cpePrefix: 'cpe:/a:google:guava',
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache 2.0'),
            com_google_testparameterinjector_test_parameter_injector: new PropertyOverride(
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache 2.0'),
            com_squareup_wire_wire_runtime_jvm: new PropertyOverride(
                    licenseUrl: 'https://www.apache.org/licenses/LICENSE-2.0.txt',
                    licenseName: 'Apache 2.0'),
            org_bouncycastle_bcprov_jdk18on: new PropertyOverride(
                    cpePrefix: 'cpe:/a:bouncycastle:legion-of-the-bouncy-castle:1.72',
                    licensePath: 'licenses/Bouncy_Castle-2015.txt',
                    licenseName: 'MIT'),
            org_codehaus_mojo_animal_sniffer_annotations: new PropertyOverride(
                    description: 'Animal Sniffer Annotations allow marking methods which Animal Sniffer should ignore ' +
                            'signature violations of.',
                    licenseUrl: 'https://raw.githubusercontent.com/mojohaus/animal-sniffer/master/animal-sniffer-annotations/pom.xml',
                    licensePath: 'licenses/Codehaus_License-2009.txt',
                    licenseName: 'MIT'),
            com_google_protobuf_protobuf_lite: new PropertyOverride(
                    exclude: true, // There is a phantom dep on this target, but this is deprecated and not used in chrome.
                    licenseUrl: 'https://raw.githubusercontent.com/protocolbuffers/protobuf/master/LICENSE',
                    licenseName: 'BSD'),
            com_google_protobuf_protobuf_javalite: new PropertyOverride(
                    cpePrefix: 'cpe:/a:google:protobuf-javalite',
                    licenseUrl: 'https://raw.githubusercontent.com/protocolbuffers/protobuf/master/LICENSE',
                    licenseName: 'BSD'),
            io_grpc_grpc_android: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_binder: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_api: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_context: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_core: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_protobuf_lite: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_stub: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_testing: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_inprocess: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_grpc_grpc_util: new PropertyOverride(
                    cpePrefix: 'cpe:/a:grpc:grpc',
                    licenseUrl: 'https://raw.githubusercontent.com/grpc/grpc-java/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            io_perfmark_perfmark_api: new PropertyOverride(
                    licenseUrl: 'https://raw.githubusercontent.com/perfmark/perfmark/refs/heads/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            jakarta_inject_jakarta_inject_api: new PropertyOverride(
                    // Help gradle resolve the same version that our 3pp script does.
                    versionFilter: '\\d+\\.\\d+\\.\\d+$'),
            javax_annotation_javax_annotation_api: new PropertyOverride(
                    isShipped: false,  // Annotations are stripped by R8.
                    licenseName: 'CDDL-1.1, GPL-2.0-with-classpath-exception',
                    licenseUrl: 'https://raw.githubusercontent.com/javaee/javax.annotation/refs/heads/master/LICENSE'),
            javax_annotation_jsr250_api: new PropertyOverride(
                    isShipped: false,  // Annotations are stripped by R8.
                    licenseName: 'CDDL-1.0',
                    licensePath: 'licenses/CDDL-1.0.txt'),
            net_bytebuddy_byte_buddy: new PropertyOverride(
                    licenseUrl: 'https://raw.githubusercontent.com/raphw/byte-buddy/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            net_bytebuddy_byte_buddy_agent: new PropertyOverride(
                    licenseUrl: 'https://raw.githubusercontent.com/raphw/byte-buddy/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            net_bytebuddy_byte_buddy_android: new PropertyOverride(
                    licenseUrl: 'https://raw.githubusercontent.com/raphw/byte-buddy/master/LICENSE',
                    licenseName: 'Apache 2.0'),
            org_checkerframework_checker_compat_qual: new PropertyOverride(
                    licenseUrl: 'https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt',
                    licenseName: 'Apache-2.0, MIT, GPL-2.0-with-classpath-exception'),
            org_checkerframework_checker_qual: new PropertyOverride(
                    licenseUrl: 'https://raw.githubusercontent.com/typetools/checker-framework/master/LICENSE.txt',
                    licenseName: 'Apache-2.0, MIT, GPL-2.0-with-classpath-exception'),
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
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_junit: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_nativeruntime: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_nativeruntime_dist_compat: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_pluginapi: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_plugins_maven_dependency_resolver: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_resources: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_robolectric: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_sandbox: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_shadowapi: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_shadows_framework: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_utils: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            org_robolectric_utils_reflector: new PropertyOverride(
                    licenseName: 'Apache-2.0, MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/robolectric/robolectric/master/LICENSE'),
            // Prevent version changing ~weekly. https://crbug.com/1257197
            org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm: new PropertyOverride(
                    resolveVersion: '1.8.1'),
            org_jetbrains_kotlinx_kotlinx_coroutines_android: new PropertyOverride(
                    resolveVersion: '1.8.1'),
            org_jetbrains_kotlinx_kotlinx_coroutines_guava: new PropertyOverride(
                    resolveVersion: '1.8.1'),
            org_jetbrains_kotlinx_kotlinx_serialization_core_jvm: new PropertyOverride(
                    resolveVersion: '1.7.2'),
            org_jetbrains_kotlinx_kotlinx_serialization_json: new PropertyOverride(
                    resolveVersion: '1.7.2', overrideLatest: true),
            org_jetbrains_kotlinx_kotlinx_coroutines_test_jvm: new PropertyOverride(
                    resolveVersion: '1.7.3'),
            io_reactivex_rxjava3_rxjava: new PropertyOverride(
                    exclude: true),  // An unnecessary dep of androidx.xr.runtime.
            org_jetbrains_kotlinx_kotlinx_coroutines_reactive: new PropertyOverride(
                    exclude: true),  // An unnecessary dep of androidx.xr.runtime.
            org_jetbrains_kotlinx_kotlinx_coroutines_rx3: new PropertyOverride(
                    exclude: true),  // An unnecessary dep of androidx.xr.runtime.
            org_reactivestreams_reactive_streams: new PropertyOverride(
                    licenseName: 'MIT',
                    licenseUrl: 'https://raw.githubusercontent.com/reactive-streams/reactive-streams-jvm/refs/tags/v1.0.4/LICENSE'),

    ]

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

    final Map<String, DependencyDescription> dependencies = [:] as ConcurrentHashMap<String, DependencyDescription>
    Project[] projects
    Logger logger
    boolean skipLicenses
    boolean warnOnStaleDeps

    // TODO: remove (set to true) when AUTOROLL_MIGRATION_IN_PROGRESS = false
    boolean tagTargetsAsAutorolled

    private static String makeModuleIdInner(String group, String module, String version) {
        // Does not include version because by default the resolution strategy for gradle is to use the newest version
        // among the required ones. We want to be able to match it in the BUILD.gn file.
        String moduleId = sanitize("${group}_${module}")
        return moduleId
    }

    static String makeModuleId(ResolvedModuleVersion module) {
        return makeModuleIdInner(module.id.group, module.id.name, module.id.version)
    }

    static String makeModuleId(ResolvedArtifact artifact) {
        ComponentIdentifier componentId = artifact.id.componentIdentifier
        return makeModuleIdInner(componentId.group, componentId.module, componentId.version)
    }

    static String makeModuleId(ResolvedDependencyResult dependency) {
        ComponentIdentifier componentId = dependency.selected.id
        return makeModuleIdInner(componentId.group, componentId.module, componentId.version)
    }

    static boolean anyContains(String value, Set<String>... sets) {
        for (Set<String> curSet : sets) {
            if (curSet.contains(value)) {
                return true;
            }
        }
        return false;
    }

    void collectDependencies() {
        Set<ResolvedDependency> deps = [] as Set
        Map<String, SortedSet<String>> resolvedDeps = [:]
        ExecutorService downloadExecutor = Executors.newCachedThreadPool()
        List<Future> futures = []
        String[] configNames = [
                'compile',
                'compileLatest',
                'supportsAndroidCompile',
                'supportsAndroidCompileLatest',
                'buildCompile',
                'buildCompileLatest',
                'testCompile',
                'testCompileLatest',
                'androidTestCompile',
                'androidTestCompileLatest',
                'buildCompileNoDeps'
        ]
        timeIt('** Resolving all deps') {
            for (Project project : projects) {
                for (String configName : configNames) {
                    Configuration configuration = project.configurations.getByName(configName)
                    deps += configuration.resolvedConfiguration.firstLevelModuleDependencies
                    if (!resolvedDeps.containsKey(configName)) {
                        resolvedDeps[configName] = [] as SortedSet
                    }
                    configuration.incoming.resolutionResult.allDependencies { DependencyResult dr ->
                        if (dr instanceof ResolvedDependencyResult) {
                            resolvedDeps[configName] += makeModuleId(dr)
                        } else {
                            // We don't currently have any unresolved deps, though it is a potential return type of
                            // ResolutionResult#allDependencies, see:
                            // https://docs.gradle.org/current/javadoc/org/gradle/api/artifacts/result/ResolutionResult.html#getAllDependencies()
                            logger.warn("Unresolved ${dr.from} -> ${dr.requested}")
                        }
                    }
                }
            }
        }

        List<String> topLevelIds = []
        Map<String, ResolvedDependency> resolvedVersions = [:]
        deps.each { dependency ->
            topLevelIds.add(makeModuleId(dependency.module))
            resolveVersionRecursive(dependency, resolvedVersions)
        }
        ExecutorService taskExecutor = Executors.newCachedThreadPool()
        List<Future> taskList = []
        timeIt("** Parse all pom files") {
            buildDepDescriptionsAsync(resolvedVersions, taskExecutor, taskList)
            taskExecutor.shutdown()
            taskList.each { task -> task.get() }
        }

        // Collect these using prefix match to allow variants "Latest", "NoDeps", "Autorolled".
        Set<String> compileIds = [] as Set
        Set<String> testIds = [] as Set
        Set<String> androidTestIds = [] as Set
        Set<String> buildIds = [] as Set
        Set<String> supportsIds = [] as Set
        Set<String> autorolledIds = [] as Set
        resolvedDeps.each { key, values ->
            if (key.startsWith('compile')) {
                compileIds.addAll(values);
            } else if (key.startsWith('testCompile')) {
                testIds.addAll(values);
            } else if (key.startsWith('androidTest')) {
                androidTestIds.addAll(values);
            } else if (key.startsWith('build')) {
                buildIds.addAll(values);
            } else if (key.startsWith('supportsAndroid')) {
                supportsIds.addAll(values);
            } else {
                assert false : 'Unknown config ' + key
            }
            if (tagTargetsAsAutorolled && key.endsWith("Latest")) {
                autorolledIds.addAll(values)
            }
        }

        dependencies.each { id, dep ->
            dep.visible = topLevelIds.contains(id)
            // These lists of ids contain all transitive deps of a target of this type. So, for
            // robolectric and testonly, we only want ids that match, but aren't also matched by
            // another group that would prevent it from being robolectric or testonly.
            dep.isRobolectric = anyContains(id, testIds) &&
                                !anyContains(id, compileIds, androidTestIds, buildIds, supportsIds)
            dep.testOnly = anyContains(id, androidTestIds, testIds) &&
                           !anyContains(id, compileIds, buildIds, supportsIds)
            dep.supportsAndroid = anyContains(id, compileIds, androidTestIds, supportsIds)
            dep.requiresAndroid = dep.supportsAndroid && !anyContains(id, buildIds, supportsIds)
            dep.usedInBuild = anyContains(id, buildIds)
            dep.isShipped = dep.supportsAndroid && !dep.testOnly
            dep.isAutorolled = anyContains(id, autorolledIds)
        }

        // Find all reachable deps and mark unreachable ones as excluded.
        // Required to prune deps of excluded deps.
        Set<String> seen = new HashSet<>(topLevelIds);
        seen.addAll(BuildConfigGenerator.EXISTING_LIBS.keySet());
        ArrayList<String> workList = new ArrayList<>(topLevelIds);
        while (!workList.isEmpty()) {
          String id = workList.remove(workList.size() - 1);
          DependencyDescription dep = dependencies.get(id)
          dep.children.each { childId ->
            DependencyDescription childDep = dependencies.get(childId)
            if (!childDep.exclude && seen.add(childId)) {
              workList.add(childId);
            }
          }
        }

        dependencies.each { id, dep ->
          if (!seen.contains(id)) {
            dep.exclude = true
          }
        }

        PROPERTY_OVERRIDES.each { id, overrides ->
            DependencyDescription dep = dependencies.get(id)
            if (dep) {
                // Null-check is required since isShipped is a boolean. This check must come after all the deps are
                // resolved instead of in customizeDep, since otherwise it gets overwritten.
                if (overrides.isShipped != null) {
                    dep.isShipped = overrides.isShipped
                }
                if (overrides.supportsAndroid != null) {
                    dep.supportsAndroid = overrides.supportsAndroid
                }
                // If overrideLatest is true, set it recursively on the dep and all its children. This is convenient
                // since you do not have to set it on a whole set of old deps.
                if (overrides.overrideLatest) {
                    recursivelyOverrideLatestVersion(dep)
                }
                dep.versionFilter = overrides.versionFilter
            } else {
                if (warnOnStaleDeps) {
                    logger.warn('PROPERTY_OVERRIDES has stale dep: ' + id)
                }
            }
        }
    }

    <T> T timeIt(boolean enable, String actionName, Closure<T> closure) {
        if (enable) {
            return timeIt(actionName, closure)
        }
        return closure()
    }

    <T> T timeIt(String actionName, Closure<T> closure) {
        def start = Instant.now()
        def ret = closure()
        def elapsed = Duration.between(start, Instant.now()).toMillis()
        logger.warn "${actionName} took ${elapsed} ms"
        return ret
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

    private static boolean areAllModuleArtifactsSameFile(Set<ResolvedArtifact> artifacts) {
        if (artifacts.size() == 1) return true
        String expectedPath = artifacts[0].file.absolutePath
        for (ResolvedArtifact artifact : artifacts) {
            if (expectedPath != artifact.file.absolutePath) {
                return false
            }
        }
        return true
    }

    private void resolveVersionRecursive(ResolvedDependency dependency, Map<String, ResolvedDependency> resolvedVersions) {
        String id = makeModuleId(dependency.module)
        if (resolvedVersions.containsKey(id)) {
            String gotVersion = dependency.module.id.version
            if (resolvedVersions.get(id).module.id.version == gotVersion) {
                return
            }
            PropertyOverride overrides = PROPERTY_OVERRIDES.get(id)
            if (overrides?.resolveVersion) {
                if (overrides.resolveVersion != gotVersion) {
                    return
                }
            } else if (isVersionLower(gotVersion, resolvedVersions.get(id).module.id.version)) {
                // Default to using largest version for version conflict resolution. See http://crbug.com/1040958.
                // https://docs.gradle.org/current/userguide/dependency_resolution.html#sec:version-conflict
                return
            }
        }

        resolvedVersions.put(id, dependency)

        dependency.children.each { it -> resolveVersionRecursive(it, resolvedVersions) }
    }

    private void buildDepDescriptionsAsync(Map<String, ResolvedDependency> resolvedDeps, ExecutorService taskExecutor, List<Future> taskList) {
        resolvedDeps.each { String id, ResolvedDependency dependency ->
            List<String> childModules = []

            dependency.children.each { it ->
                childModules += makeModuleId(it.module)
            }

            if (dependency.moduleArtifacts.empty) {
                taskList.add(taskExecutor.submit {
                    dependencies.put(id, buildDepDescriptionNoArtifact(id, dependency, childModules))
                })
            } else if (!areAllModuleArtifactsSameFile(dependency.moduleArtifacts)) {
                throw new IllegalStateException("The dependency ${id} has multiple different artifacts: " +
                        "${dependency.moduleArtifacts}")
            } else {
                ResolvedArtifact artifact = dependency.moduleArtifacts[0]
                if (artifact.extension != 'jar' && artifact.extension != 'aar') {
                    throw new IllegalStateException("Type ${artifact.extension} of ${id} not supported.")
                }
                taskList.add(taskExecutor.submit {
                    dependencies.put(id, buildDepDescription(id, dependency, artifact, childModules))
                })
            }
        }
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
                exclude: childModules.isEmpty(),
                cipdSuffix: DEFAULT_CIPD_SUFFIX,
        ))
    }

    private DependencyDescription buildDepDescription(
            String id, ResolvedDependency dependency, ResolvedArtifact artifact, List<String> childModules) {
        String pomUrl, repoUrl, fileUrl, description, displayName
        GPathResult pomContent
        List<LicenseSpec> licenses = []
        (repoUrl, pomUrl, pomContent) = computePomFromArtifact(artifact)

        if (!skipLicenses) {
            licenses = resolveLicenseInformation(pomContent)
        }

        // Build |fileUrl| by swapping '.pom' file extension with artifact file extension.
        fileUrl = pomUrl[0..-4] + artifact.extension
        // Check that the URL is correct explicitly here. Otherwise, we won't
        // find out until 3pp bot runs.
        checkDownloadable(fileUrl)

        // Get rid of irrelevant indent that might be present in the XML file.
        description = pomContent.description?.text()?.trim()?.replaceAll(/\s+/, ' ')
        displayName = pomContent.name?.text()
        displayName = displayName ?: dependency.module.id.name

        String url = pomContent.url?.text()
        if (url) {
          if (description) {
            description += "\n\n"
          } else {
            description = ""
          }
          description += "See also: " + url + "\n"
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
                licenses: licenses,
                directoryName: id.toLowerCase(),
                fileName: dependency.module.id.name + "." + artifact.extension,
                fileUrl: fileUrl,
                repoUrl: repoUrl,
                description: description,
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
                        name: overrides.licenseName,
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
        if (dep.id?.startsWith('androidx_')) {
            // By default androidx dependencies' licenses are compatible with android.
            dep.licenseAndroidCompatible = true
        }

        if (!dep.description && dep.id) {
            // Some libraries do not come with a description. The only description we have for most
            // of them is the name of the lib so might as well automate a fallback.
            String lib_name = dep.id
            String description = "pulled in via gradle."
            // Removing common prefixes.
            if (lib_name.startsWith('com_') || lib_name.startsWith('org_')) {
                lib_name = lib_name.substring('com_'.length())
            }
            if (lib_name.startsWith('google_')) {
                lib_name = lib_name.substring('google_'.length())
            }
            if (lib_name.startsWith('android_')) {
                lib_name = lib_name.substring('android_'.length())
            }
            if (lib_name.startsWith('gms_play_services_')) {
                lib_name = lib_name.substring('gms_play_services_'.length())
                description = "library for gmscore / Google Play Services."
            }
            lib_name = lib_name.replace('_', ' ').capitalize()
            dep.description = "$lib_name $description"
        }

        PropertyOverride overrides = PROPERTY_OVERRIDES.get(dep.id)
        if (overrides) {
            logger.debug("Using override properties for $dep.id")
            dep.with {
                description = overrides.description ?: description
                cipdSuffix = overrides.cipdSuffix ?: cipdSuffix
                cpePrefix = overrides.cpePrefix ?: cpePrefix
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

    private static List<LicenseSpec> resolveLicenseInformation(GPathResult pomContent) {
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
        String componentPomSubPath = String.format('%s/%s/%s/%s-%s.pom',
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
            String fileUrl = String.format('%s/%s', repoUrl, componentPomSubPath)
            try {
                GPathResult content = new XmlSlurper(
                        false /* validating */, false /* namespaceAware */).parse(fileUrl)
                logger.debug("Succeeded in resolving url $fileUrl")
                return [repoUrl, fileUrl, content]
            } catch (ignored) {
                logger.debug("Failed in resolving url $fileUrl")
            }
        }
        throw new RuntimeException("Could not find pom from artifact $componentPomSubPath in $repoUrls")
    }

    private static void checkDownloadable(String url) {
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
                new RuntimeException("Resolved POM but could not resolve $url").printStackTrace()
                // Exception is logged and ignored if thrown, so explicitly exit.
                System.exit(1)
            }
            http.disconnect()
        })
    }

    // Checks if currentVersion is lower than versionInQuestion.
    private boolean isVersionLower(String currentVersion, String versionInQuestion) {
        List verA = currentVersion.tokenize('.-')
        List verB = versionInQuestion.tokenize('.-')
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
            } catch (ignored) {
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
        String group, name, version, extension, displayName, description
        List<LicenseSpec> licenses
        String fileName, fileUrl
        // |repoUrl| is the url to the repo that hosts this dep's artifact (|fileUrl|). Basically
        // |fileUrl|.startsWith(|repoUrl|).
        String repoUrl
        // The local directory name to store the files like artifact, license file, 3pp subdirectory, and etc. Must be
        // lowercase since 3pp uses the directory name as part of the CIPD names. However CIPD does not allow uppercase
        // in names.
        String directoryName
        boolean visible, exclude, testOnly, isShipped, usedInBuild
        boolean supportsAndroid
        boolean requiresAndroid
        boolean isRobolectric
        boolean isAutorolled = false
        boolean licenseAndroidCompatible
        ComponentIdentifier componentId
        List<String> children
        String cipdSuffix
        String cpePrefix
        // The fetch_all.py normally fetches the latest version instead of the declared version in build.gradle. When
        // overrideLatest is set to true, the actual version resolved by gradle (based on what is declared in
        // build.gradle as well as the version other dependencies need) will be used.
        Boolean overrideLatest
        // When set, //third_party/android_deps/fetch_common.py will only versions that contain this string to be valid.
        // This variable is not used in groovy code.
        String versionFilter

        String getDirectoryPath() {
            return BuildConfigGenerator.LIBS_DIRECTORY + '/' + directoryName
        }

        String getArtifactDirectoryPath() {
            if (artifactPrefix) {
                return "$artifactPrefix/$directoryPath"
            }
            return directoryPath
        }

        String getArtifactPrefix() {
            // All artifacts live under cipd/ in all projects
            return 'cipd'
        }

        String getCommittedDirectoryPath() {
            if (committedPrefix) {
                return "$committedPrefix/$directoryPath"
            }
            return directoryPath
        }

        String getCommittedPrefix() {
            if (isAndroidx || isAutorolled) {
                return 'committed'
            }
            // Main project does not have committed subdir since it is all
            // "committed".
            return null
        }

        String getRebasePrefix(String basePath) {
            return Paths.get(basePath).relativize(Paths.get(this.projectPath)).toString()
        }

        // When writing the BUILD.gn, the paths of autorolled deps is rebased
        // with respect to the main project path since the autorolled BUILD.gn
        // is imported to the main BUILD.gn
        String getRebasedCommittedDirectoryPath(String currentProjectPath) {
            if (currentProjectPath == this.projectPath) {
                return this.committedDirectoryPath
            }
            String rebasePrefix = getRebasePrefix(currentProjectPath)
            return "${rebasePrefix}/$committedDirectoryPath"
        }

        String getRebasedArtifactDirectoryPath(String currentProjectPath) {
            if (currentProjectPath == this.projectPath) {
                return this.artifactDirectoryPath
            }
            String rebasePrefix = getRebasePrefix(currentProjectPath)
            return "${rebasePrefix}/$artifactDirectoryPath"
        }

        boolean getIsAndroidx() {
            return id.startsWith('androidx')
        }

        // This indicates which build.gradle subproject the target belongs to.
        // Used to determine whether to process the target for this run.
        String getProjectPath() {
            if (isAndroidx) {
                return BuildConfigGenerator.ANDROIDX_PROJECT_PATH
            }
            if (isAutorolled) {
                return BuildConfigGenerator.AUTOROLLED_PROJECT_PATH
            }
            return BuildConfigGenerator.MAIN_PROJECT_PATH
        }

        // Indicates which BUILD.gn file the target lives in
        String getBuildGnPath() {
            if (isAndroidx) {
                return BuildConfigGenerator.ANDROIDX_PROJECT_PATH
            }
            // While autorolled targets are generated as part of
            // AUTOROLLED_PROJECT_PATH's BUILD.gn, it is declared inside a gn
            // template to be imported into the MAIN_PROJECT_PATH's BUILD.gn.
            return BuildConfigGenerator.MAIN_PROJECT_PATH
        }
    }

    static class LicenseSpec {
        String name, url, path
    }

    static class PropertyOverride {
        String description
        String licenseName, licenseUrl, licensePath
        String cipdSuffix
        String cpePrefix
        String resolveVersion
        Boolean isShipped
        Boolean supportsAndroid
        // Set to true if this dependency is not needed.
        Boolean exclude
        Boolean overrideLatest
        String versionFilter
    }
}
