// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.apksig.util.RunnablesExecutor;
import com.android.signflinger.SignedApk;
import com.android.signflinger.SignedApkOptions;
import com.android.zipflinger.Archive;
import com.android.zipflinger.Source;
import com.android.zipflinger.Sources;
import com.android.zipflinger.ZipArchive;
import com.android.zipflinger.ZipSource;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/**
 * Creates a zip file (optionally an APK signed with v2 + v4 signatures) from a "spec" file that
 * lists the entries to add.
 *
 * <p>Usage:
 *
 * <pre>
 * ZipBuilder --output OUT --spec SPEC \
 *     [--keystore KS --key-alias ALIAS --key-password PW --min-sdk-version N]
 * </pre>
 *
 * <p>Each line of the spec describes one entry and has tab-separated fields:
 *
 * <pre>
 * DEST_NAME \t COMPRESS_LEVEL \t ALIGNMENT \t SRC_PATH [\t SRC_ZIP_ENTRY]
 * </pre>
 *
 * <ul>
 *   <li>Entries are written in the order they are listed.
 *   <li>When SRC_ZIP_ENTRY is omitted, SRC_PATH is a file to add. COMPRESS_LEVEL is 0 (stored)
 *       through 9.
 *   <li>When SRC_ZIP_ENTRY is present, SRC_PATH is a zip file and the named entry is copied from
 *       it. COMPRESS_LEVEL of -1 copies the compressed bytes as-is, 0 stores it, and 1-9
 *       recompresses it.
 *   <li>ALIGNMENT aligns the entry's data to the given byte boundary within the output (0 for no
 *       alignment).
 * </ul>
 *
 * <p>When --keystore is given, the output is signed using signflinger (which wraps apksig), so that
 * the archive is written only once rather than once by the zip tool and again by apksigner. The v4
 * signature is written to {@code OUT.idsig} (e.g. {@code foo.apk.idsig}).
 *
 * <p>See 3pp/3pp.py for why this runs on the JVM rather than as a GraalVM native-image.
 */
public final class ZipBuilder {
    // apksig defaults to one digest thread per core. Beyond ~8 threads, wall
    // time does not improve but total CPU time grows substantially, which
    // hurts build throughput when many actions run concurrently.
    private static final int MAX_THREADS = Math.min(8, Runtime.getRuntime().availableProcessors());

    private static final class Options {
        Path output;
        Path spec;
        Path keystore;
        String keyAlias;
        String keyPassword;
        int minSdkVersion = -1;
    }

    private static Options parseArgs(String[] args) {
        Options options = new Options();
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if (i + 1 >= args.length) {
                throw new IllegalArgumentException("Missing value for " + arg);
            }
            String value = args[++i];
            switch (arg) {
                case "--output":
                    options.output = Paths.get(value);
                    break;
                case "--spec":
                    options.spec = Paths.get(value);
                    break;
                case "--keystore":
                    options.keystore = Paths.get(value);
                    break;
                case "--key-alias":
                    options.keyAlias = value;
                    break;
                case "--key-password":
                    options.keyPassword = value;
                    break;
                case "--min-sdk-version":
                    options.minSdkVersion = Integer.parseInt(value);
                    break;
                default:
                    throw new IllegalArgumentException("Unknown argument: " + arg);
            }
        }
        if (options.output == null || options.spec == null) {
            throw new IllegalArgumentException("--output and --spec are required");
        }
        if (options.keystore != null) {
            if (options.keyAlias == null
                    || options.keyPassword == null
                    || options.minSdkVersion < 0) {
                throw new IllegalArgumentException(
                        "--keystore requires --key-alias, --key-password, and"
                                + " --min-sdk-version");
            }
            // v1 signing is required below 24 and is not supported here.
            if (options.minSdkVersion < 24) {
                throw new IllegalArgumentException("--min-sdk-version must be >= 24");
            }
        }
        return options;
    }

    /**
     * Parses the spec file into a list of items to add. Items are either a {@code Future<Source>}
     * (loose file, prepared on a thread pool since computing the CRC and compressing is the bulk of
     * the work) or a {@code ZipSource} (a run of consecutive entries copied from one zip).
     */
    private static Deque<Object> parseSpec(Path specPath, ExecutorService pool) throws IOException {
        Deque<Object> items = new ArrayDeque<>();
        ZipSource currentZip = null;
        Path currentZipPath = null;
        int lineNumber = 0;
        for (String line : Files.readAllLines(specPath, StandardCharsets.UTF_8)) {
            lineNumber++;
            if (line.isEmpty()) {
                continue;
            }
            String[] fields = line.split("\t", -1);
            boolean fromZip = fields.length == 5;
            if (fields.length != 4 && !fromZip) {
                throw new IllegalArgumentException(
                        specPath + ":" + lineNumber + ": Expected 4 or 5 fields: " + line);
            }
            String destName = fields[0];
            int compressLevel = Integer.parseInt(fields[1]);
            long alignment = Long.parseLong(fields[2]);
            Path srcPath = Paths.get(fields[3]);
            if (compressLevel < (fromZip ? -1 : 0) || compressLevel > 9) {
                throw new IllegalArgumentException(
                        specPath + ":" + lineNumber + ": Invalid compress level: " + line);
            }

            if (!fromZip) {
                currentZip = null;
                items.add(
                        pool.submit(
                                () -> {
                                    Source source = Sources.from(srcPath, destName, compressLevel);
                                    if (alignment > 0) {
                                        source.align(alignment);
                                    }
                                    return source;
                                }));
            } else {
                if (currentZip == null || !srcPath.equals(currentZipPath)) {
                    currentZipPath = srcPath;
                    currentZip = new ZipSource(srcPath);
                    items.add(currentZip);
                }
                if (currentZip.entries().get(fields[4]) == null) {
                    throw new IllegalArgumentException(
                            specPath + ":" + lineNumber + ": Entry not found in zip: " + line);
                }
                int level = compressLevel < 0 ? ZipSource.COMPRESSION_NO_CHANGE : compressLevel;
                long align = alignment > 0 ? alignment : Source.NO_ALIGNMENT;
                currentZip.select(fields[4], destName, level, align);
            }
        }
        return items;
    }

    private static Archive createArchive(Options options, ExecutorService pool) throws Exception {
        if (options.keystore == null) {
            return new ZipArchive(options.output);
        }

        char[] password = options.keyPassword.toCharArray();
        KeyStore keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
        try (InputStream in = Files.newInputStream(options.keystore)) {
            keyStore.load(in, password);
        }
        PrivateKey privateKey = (PrivateKey) keyStore.getKey(options.keyAlias, password);
        if (privateKey == null) {
            throw new IllegalArgumentException("Key alias not found: " + options.keyAlias);
        }
        List<X509Certificate> certificates = new ArrayList<>();
        for (Certificate cert : keyStore.getCertificateChain(options.keyAlias)) {
            certificates.add((X509Certificate) cert);
        }

        // Signing happens in Archive.close(), by which point the pool is idle.
        RunnablesExecutor executor =
                provider -> {
                    List<Future<?>> futures = new ArrayList<>();
                    for (int i = 0; i < MAX_THREADS; i++) {
                        futures.add(pool.submit(provider.createRunnable()));
                    }
                    try {
                        for (Future<?> future : futures) {
                            future.get();
                        }
                    } catch (InterruptedException | ExecutionException e) {
                        throw new RuntimeException(e);
                    }
                };

        // v3 signing adds security niceties that are irrelevant for local
        // builds. v4 signatures (.idsig files) make "adb install" faster.
        SignedApkOptions signedApkOptions =
                new SignedApkOptions.Builder()
                        .setName("CERT")
                        .setPrivateKey(privateKey)
                        .setCertificates(certificates)
                        .setExecutor(executor)
                        .setV1Enabled(false)
                        .setV2Enabled(true)
                        .setV3Enabled(false)
                        .setV4Enabled(true)
                        .setV4Output(idsigPath(options).toFile())
                        .setMinSdkVersion(options.minSdkVersion)
                        .build();
        return new SignedApk(options.output.toFile(), signedApkOptions);
    }

    private static Path idsigPath(Options options) {
        return options.output.resolveSibling(options.output.getFileName() + ".idsig");
    }

    private static void deleteOutputs(Options options) throws IOException {
        Files.deleteIfExists(options.output);
        Files.deleteIfExists(idsigPath(options));
    }

    public static void main(String[] args) throws Exception {
        Options options = parseArgs(args);
        ExecutorService pool = Executors.newFixedThreadPool(MAX_THREADS);
        try {
            Deque<Object> items = parseSpec(options.spec, pool);
            // zipflinger opens existing files for in-place modification.
            deleteOutputs(options);
            try (Archive archive = createArchive(options, pool)) {
                // Poll rather than iterate so that each source's in-memory
                // payload can be garbage collected once it has been written.
                Object item;
                while ((item = items.pollFirst()) != null) {
                    if (item instanceof ZipSource) {
                        archive.add((ZipSource) item);
                    } else {
                        @SuppressWarnings("unchecked")
                        Source source = ((Future<Source>) item).get();
                        archive.add(source);
                    }
                }
            } catch (Exception e) {
                // Do not leave behind a partially written output.
                deleteOutputs(options);
                throw e;
            }
        } finally {
            pool.shutdownNow();
        }
    }
}
