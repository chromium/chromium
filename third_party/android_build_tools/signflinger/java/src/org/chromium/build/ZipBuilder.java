// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.apksig.KeyConfig;
import com.android.apksig.internal.apk.ApkSigningBlockUtils;
import com.android.apksig.internal.apk.ContentDigestAlgorithm;
import com.android.apksig.internal.apk.SignatureAlgorithm;
import com.android.apksig.internal.apk.v2.V2SchemeSigner;
import com.android.apksig.internal.apk.v3.V3SchemeSigner;
import com.android.apksig.internal.apk.v4.V4SchemeSigner;
import com.android.apksig.internal.apk.v4.V4Signature;
import com.android.apksig.internal.util.Pair;
import com.android.apksig.internal.zip.ZipUtils;
import com.android.apksig.util.RunnablesExecutor;
import com.android.signflinger.SignedApk;
import com.android.signflinger.SignedApkOptions;
import com.android.zipflinger.Archive;
import com.android.zipflinger.Entry;
import com.android.zipflinger.Source;
import com.android.zipflinger.Sources;
import com.android.zipflinger.ZipArchive;
import com.android.zipflinger.ZipInfo;
import com.android.zipflinger.ZipMap;
import com.android.zipflinger.ZipSource;
import com.android.zipflinger.ZipWriter;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.Deque;
import java.util.EnumMap;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.CRC32;

/**
 * Creates a zip file (optionally an APK signed with v1/v2/v3/v4 signatures) from a "spec" file that
 * lists the entries to add.
 *
 * <p>Usage:
 *
 * <pre>
 * ZipBuilder --output OUT --spec SPEC \
 *     [--keystore KS --key-alias ALIAS --key-password PW --min-sdk-version N \
 *      --v1-signing-enabled BOOL --v2-signing-enabled BOOL \
 *      --v3-signing-enabled BOOL --v4-signing-enabled BOOL]
 * </pre>
 *
 * <p>Each line of the spec describes one entry (or all remaining entries from a zip) and has
 * tab-separated fields:
 *
 * <pre>
 * DEST_NAME \t COMPRESS_LEVEL \t ALIGNMENT \t SRC_PATH [\t SRC_ZIP_ENTRY]
 * </pre>
 *
 * <ul>
 *   <li>Entries are written in the order they are listed.
 *   <li>When SRC_ZIP_ENTRY is omitted (4 fields), SRC_PATH is a file to add. COMPRESS_LEVEL is 0
 *       (stored) through 9.
 *   <li>When SRC_ZIP_ENTRY is present (5 fields), SRC_PATH is a zip file. If SRC_ZIP_ENTRY is
 *       non-empty, that entry is copied to DEST_NAME. If SRC_ZIP_ENTRY is empty, all remaining
 *       non-directory entries in SRC_PATH (not already copied by an earlier line) are copied in
 *       sorted order with DEST_NAME prepended as a prefix. COMPRESS_LEVEL of -1 copies the
 *       compressed bytes as-is, 0 stores it, and 1-9 recompresses it.
 *   <li>ALIGNMENT aligns uncompressed entries' data to the given byte boundary within the output (0
 *       for no alignment; ignored for compressed entries).
 * </ul>
 *
 * <p>When --keystore is given, the output is signed using signflinger/apksig, so that the archive
 * is written only once rather than once by the zip tool and again by apksigner. When v4 signing is
 * enabled, the v4 signature is written to {@code OUT.idsig} (e.g. {@code foo.apk.idsig}).
 */
public final class ZipBuilder {
    // apksig defaults to one digest thread per core. Beyond ~8 threads, wall
    // time does not improve but total CPU time grows substantially, which
    // hurts build throughput when many actions run concurrently.
    private static final int MAX_THREADS = Math.min(8, Runtime.getRuntime().availableProcessors());

    // Loose stored files >= 64 KiB use StoredFileSource rather than Sources.from()
    // because zipflinger's BytesSource reads the entire file onto the Java heap
    // (up to 100 MB) and LargeFileSource (> 100 MB) computes CRC32 via a 4 KiB
    // CheckedInputStream buffer (~113k read syscalls on libchrome.so).
    private static final int STORED_FILE_THRESHOLD = 64 * 1024;
    private static final int DIRECT_BUFFER_SIZE = 256 * 1024;
    private static final int V2_CHUNK_SIZE = 1024 * 1024;
    private static final int V4_PAGE_SIZE = 4096;
    private static final int SHA256_DIGEST_SIZE = 32;

    private static final ThreadLocal<ByteBuffer> DIRECT_BUF =
            ThreadLocal.withInitial(() -> ByteBuffer.allocateDirect(DIRECT_BUFFER_SIZE));

    private static final Method GENERATE_V2_BLOCK_METHOD;
    private static final Method GENERATE_V3_BLOCK_METHOD;
    private static final Constructor<V4Signature.HashingInfo> HASHING_INFO_CTOR;
    private static final Method GENERATE_V4_SIGNATURE_METHOD;

    static {
        try {
            GENERATE_V2_BLOCK_METHOD =
                    V2SchemeSigner.class.getDeclaredMethod(
                            "generateApkSignatureSchemeV2Block",
                            List.class,
                            Map.class,
                            boolean.class,
                            List.class);
            GENERATE_V2_BLOCK_METHOD.setAccessible(true);

            GENERATE_V3_BLOCK_METHOD =
                    V3SchemeSigner.class.getDeclaredMethod(
                            "generateApkSignatureSchemeV3Block", Map.class);
            GENERATE_V3_BLOCK_METHOD.setAccessible(true);

            HASHING_INFO_CTOR =
                    V4Signature.HashingInfo.class.getDeclaredConstructor(
                            int.class, byte.class, byte[].class, byte[].class);
            HASHING_INFO_CTOR.setAccessible(true);

            GENERATE_V4_SIGNATURE_METHOD =
                    V4SchemeSigner.class.getDeclaredMethod(
                            "generateSignature",
                            V4SchemeSigner.SignerConfig.class,
                            V4Signature.HashingInfo.class,
                            Map.class,
                            byte[].class,
                            long.class);
            GENERATE_V4_SIGNATURE_METHOD.setAccessible(true);
        } catch (ReflectiveOperationException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    private static final class StoredFileSource extends Source {
        private final Path mPath;

        StoredFileSource(Path path, String name, long size) throws IOException {
            super(name);
            mPath = path;
            uncompressedSize = size;
            compressedSize = size;
            compressionFlag = 0;
            CRC32 crc32 = new CRC32();
            ByteBuffer buf = DIRECT_BUF.get();
            try (FileChannel ch = FileChannel.open(path, StandardOpenOption.READ)) {
                buf.clear();
                while (ch.read(buf) != -1) {
                    buf.flip();
                    crc32.update(buf);
                    buf.clear();
                }
            }
            crc = (int) crc32.getValue();
        }

        @Override
        public void prepare() {}

        @Override
        public long writeTo(ZipWriter writer) throws IOException {
            try (FileChannel ch = FileChannel.open(mPath, StandardOpenOption.READ)) {
                writer.transferFrom(ch, 0, compressedSize);
            }
            return compressedSize;
        }
    }

    private static final class Options {
        Path output;
        Path spec;
        Path keystore;
        String keyAlias;
        String keyPassword;
        int minSdkVersion = -1;
        boolean v1SigningEnabled;
        boolean v2SigningEnabled = true;
        boolean v3SigningEnabled;
        boolean v4SigningEnabled = true;
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
                case "--v1-signing-enabled":
                    options.v1SigningEnabled = Boolean.parseBoolean(value);
                    break;
                case "--v2-signing-enabled":
                    options.v2SigningEnabled = Boolean.parseBoolean(value);
                    break;
                case "--v3-signing-enabled":
                    options.v3SigningEnabled = Boolean.parseBoolean(value);
                    break;
                case "--v4-signing-enabled":
                    options.v4SigningEnabled = Boolean.parseBoolean(value);
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
        }
        return options;
    }

    private static void selectFromZip(
            ZipSource zip, Entry entry, String destName, int compressLevel, long alignment) {
        int level = compressLevel < 0 ? ZipSource.COMPRESSION_NO_CHANGE : compressLevel;
        boolean isStored = compressLevel == 0 || (compressLevel < 0 && !entry.isCompressed());
        long align = (alignment > 0 && isStored) ? alignment : Source.NO_ALIGNMENT;
        zip.select(entry.getName(), destName, level, align);
    }

    private static Source createLooseSource(
            Path srcPath, String destName, int compressLevel, long alignment) throws IOException {
        Source source;
        if (compressLevel == 0) {
            long size = Files.size(srcPath);
            if (size >= STORED_FILE_THRESHOLD) {
                source = new StoredFileSource(srcPath, destName, size);
            } else {
                source = Sources.from(srcPath, destName, 0);
            }
            if (alignment > 0) {
                source.align(alignment);
            }
        } else {
            source = Sources.from(srcPath, destName, compressLevel);
        }
        return source;
    }

    private static final class ZipSelectionState {
        final Set<String> explicitlySelected = new HashSet<>();
        boolean allRemainingSelected;
    }

    private static final class ZipRunLine {
        final int lineNumber;
        final String rawLine;
        final String destName;
        final int compressLevel;
        final long alignment;
        final String srcEntryName;
        final Set<String> priorSelectedSnapshot;
        final boolean skipAllRemaining;

        ZipRunLine(
                int lineNumber,
                String rawLine,
                String destName,
                int compressLevel,
                long alignment,
                String srcEntryName,
                Set<String> priorSelectedSnapshot,
                boolean skipAllRemaining) {
            this.lineNumber = lineNumber;
            this.rawLine = rawLine;
            this.destName = destName;
            this.compressLevel = compressLevel;
            this.alignment = alignment;
            this.srcEntryName = srcEntryName;
            this.priorSelectedSnapshot = priorSelectedSnapshot;
            this.skipAllRemaining = skipAllRemaining;
        }
    }

    /**
     * Parses the spec file into a queue of {@code Future<?>} items (yielding either a {@link
     * Source} or a {@link ZipSource}).
     *
     * <p>To keep the largest stored entries (such as {@code libchrome.so} at the end of the spec)
     * on the critical path as early as possible, the very first item is submitted first (so the
     * main thread can immediately begin writing the output archive), followed by stored loose files
     * in reverse order, and then all remaining tasks in spec order.
     */
    private static Deque<Future<?>> parseSpec(Path specPath, ExecutorService pool)
            throws IOException {
        List<String> lines = Files.readAllLines(specPath, StandardCharsets.UTF_8);
        Deque<Future<?>> items = new ArrayDeque<>(lines.size());
        List<FutureTask<?>> priorityTasks = new ArrayList<>();
        List<FutureTask<?>> normalTasks = new ArrayList<>();
        List<FutureTask<Source>> storedLooseTasks = new ArrayList<>();

        Map<Path, FutureTask<ZipMap>> zipMaps = new HashMap<>();
        Map<Path, ZipSelectionState> selectionByZip = new HashMap<>();
        Path currentZipPath = null;
        List<ZipRunLine> currentRun = null;

        int lineNumber = 0;
        for (String line : lines) {
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
                currentZipPath = null;
                currentRun = null;
                FutureTask<Source> task =
                        new FutureTask<>(
                                () ->
                                        createLooseSource(
                                                srcPath, destName, compressLevel, alignment));
                if (items.isEmpty()) {
                    priorityTasks.add(task);
                } else if (compressLevel == 0) {
                    storedLooseTasks.add(task);
                } else {
                    normalTasks.add(task);
                }
                items.add(task);
            } else {
                FutureTask<ZipMap> mapFuture = zipMaps.get(srcPath);
                if (mapFuture == null) {
                    mapFuture = new FutureTask<>(() -> ZipMap.from(srcPath));
                    zipMaps.put(srcPath, mapFuture);
                    if (items.isEmpty()) {
                        priorityTasks.add(mapFuture);
                    } else {
                        normalTasks.add(mapFuture);
                    }
                }
                ZipSelectionState state =
                        selectionByZip.computeIfAbsent(srcPath, k -> new ZipSelectionState());
                String srcEntryName = fields[4];
                Set<String> snapshot = null;
                boolean skipAll = false;
                if (!srcEntryName.isEmpty()) {
                    state.explicitlySelected.add(srcEntryName);
                } else if (state.allRemainingSelected) {
                    skipAll = true;
                } else {
                    snapshot = new HashSet<>(state.explicitlySelected);
                    state.allRemainingSelected = true;
                }
                ZipRunLine runLine =
                        new ZipRunLine(
                                lineNumber,
                                line,
                                destName,
                                compressLevel,
                                alignment,
                                srcEntryName,
                                snapshot,
                                skipAll);
                if (currentRun == null || !srcPath.equals(currentZipPath)) {
                    currentZipPath = srcPath;
                    List<ZipRunLine> runLines = new ArrayList<>();
                    runLines.add(runLine);
                    currentRun = runLines;
                    FutureTask<ZipMap> finalMapFuture = mapFuture;
                    FutureTask<ZipSource> zipTask =
                            new FutureTask<>(
                                    () -> buildZipSource(specPath, finalMapFuture.get(), runLines));
                    if (items.isEmpty()) {
                        priorityTasks.add(zipTask);
                    } else {
                        normalTasks.add(zipTask);
                    }
                    items.add(zipTask);
                } else {
                    currentRun.add(runLine);
                }
            }
        }

        for (FutureTask<?> t : priorityTasks) {
            pool.execute(t);
        }
        for (int i = storedLooseTasks.size() - 1; i >= 0; i--) {
            pool.execute(storedLooseTasks.get(i));
        }
        for (FutureTask<?> t : normalTasks) {
            pool.execute(t);
        }
        return items;
    }

    private static ZipSource buildZipSource(
            Path specPath, ZipMap zipMap, List<ZipRunLine> runLines) {
        ZipSource zip = new ZipSource(zipMap);
        Set<String> localSelected = new HashSet<>();
        for (ZipRunLine rl : runLines) {
            if (!rl.srcEntryName.isEmpty()) {
                Entry entry = zip.entries().get(rl.srcEntryName);
                if (entry == null) {
                    throw new IllegalArgumentException(
                            specPath
                                    + ":"
                                    + rl.lineNumber
                                    + ": Entry not found in zip: "
                                    + rl.rawLine);
                }
                localSelected.add(rl.srcEntryName);
                selectFromZip(zip, entry, rl.destName, rl.compressLevel, rl.alignment);
            } else if (!rl.skipAllRemaining) {
                List<Entry> remaining = new ArrayList<>();
                for (Entry entry : zip.entries().values()) {
                    String name = entry.getName();
                    if (!entry.isDirectory()
                            && !rl.priorSelectedSnapshot.contains(name)
                            && !localSelected.contains(name)) {
                        remaining.add(entry);
                    }
                }
                remaining.sort(Comparator.comparing(Entry::getName));
                for (Entry entry : remaining) {
                    String name = entry.getName();
                    localSelected.add(name);
                    selectFromZip(zip, entry, rl.destName + name, rl.compressLevel, rl.alignment);
                }
            }
        }
        return zip;
    }

    private static RunnablesExecutor createExecutor(ExecutorService pool) {
        return provider -> {
            List<Future<?>> futures = new ArrayList<>(MAX_THREADS);
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
    }

    private static PrivateKey loadPrivateKey(Options options, List<X509Certificate> certificatesOut)
            throws Exception {
        char[] password = options.keyPassword.toCharArray();
        KeyStore keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
        try (InputStream in = Files.newInputStream(options.keystore)) {
            keyStore.load(in, password);
        }
        PrivateKey privateKey = (PrivateKey) keyStore.getKey(options.keyAlias, password);
        if (privateKey == null) {
            throw new IllegalArgumentException("Key alias not found: " + options.keyAlias);
        }
        for (Certificate cert : keyStore.getCertificateChain(options.keyAlias)) {
            certificatesOut.add((X509Certificate) cert);
        }
        return privateKey;
    }

    private static final class FastSignerState {
        final ApkSigningBlockUtils.SignerConfig v2Config;
        final ApkSigningBlockUtils.SignerConfig v3Config;
        final V4SchemeSigner.SignerConfig v4Config;
        final List<ContentDigestAlgorithm> contentDigestAlgorithms;

        FastSignerState(
                ApkSigningBlockUtils.SignerConfig v2Config,
                ApkSigningBlockUtils.SignerConfig v3Config,
                V4SchemeSigner.SignerConfig v4Config,
                List<ContentDigestAlgorithm> contentDigestAlgorithms) {
            this.v2Config = v2Config;
            this.v3Config = v3Config;
            this.v4Config = v4Config;
            this.contentDigestAlgorithms = contentDigestAlgorithms;
        }
    }

    private static ApkSigningBlockUtils.SignerConfig buildBlockSignerConfig(
            KeyConfig keyConfig,
            List<X509Certificate> certificates,
            List<SignatureAlgorithm> signatureAlgorithms,
            int minSdkVersion,
            int maxSdkVersion) {
        ApkSigningBlockUtils.SignerConfig config = new ApkSigningBlockUtils.SignerConfig();
        config.keyConfig = keyConfig;
        config.certificates = certificates;
        config.signatureAlgorithms = signatureAlgorithms;
        config.minSdkVersion = minSdkVersion;
        config.maxSdkVersion = maxSdkVersion;
        return config;
    }

    private static FastSignerState createFastSignerState(Options options) throws Exception {
        List<X509Certificate> certificates = new ArrayList<>();
        PrivateKey privateKey = loadPrivateKey(options, certificates);
        PublicKey publicKey = certificates.get(0).getPublicKey();
        KeyConfig keyConfig = new KeyConfig.Jca(privateKey);

        Set<ContentDigestAlgorithm> digestAlgorithms = EnumSet.noneOf(ContentDigestAlgorithm.class);
        ApkSigningBlockUtils.SignerConfig v2Config = null;
        if (options.v2SigningEnabled) {
            List<SignatureAlgorithm> sigAlgos =
                    V2SchemeSigner.getSuggestedSignatureAlgorithms(
                            publicKey,
                            options.minSdkVersion,
                            /* apkSigningBlockPaddingSupported= */ false,
                            /* deterministicDsaSigning= */ false);
            for (SignatureAlgorithm algo : sigAlgos) {
                digestAlgorithms.add(algo.getContentDigestAlgorithm());
            }
            v2Config =
                    buildBlockSignerConfig(
                            keyConfig,
                            certificates,
                            sigAlgos,
                            /* minSdkVersion= */ 0,
                            /* maxSdkVersion= */ 0);
        }

        ApkSigningBlockUtils.SignerConfig v3Config = null;
        if (options.v3SigningEnabled) {
            List<SignatureAlgorithm> sigAlgos =
                    V3SchemeSigner.getSuggestedSignatureAlgorithms(
                            publicKey,
                            options.minSdkVersion,
                            /* verityEnabled= */ false,
                            /* deterministicDsaSigning= */ false);
            int v3MinSdk = Integer.MAX_VALUE;
            for (SignatureAlgorithm algo : sigAlgos) {
                digestAlgorithms.add(algo.getContentDigestAlgorithm());
            }
            for (SignatureAlgorithm algo : sigAlgos) {
                int algoMinSdk = algo.getMinSdkVersion();
                if (algoMinSdk < v3MinSdk) {
                    v3MinSdk = algoMinSdk;
                    if (algoMinSdk <= options.minSdkVersion || algoMinSdk <= 28) {
                        break;
                    }
                }
            }
            v3Config =
                    buildBlockSignerConfig(
                            keyConfig,
                            certificates,
                            sigAlgos,
                            v3MinSdk,
                            /* maxSdkVersion= */ Integer.MAX_VALUE);
        }

        V4SchemeSigner.SignerConfig v4Config = null;
        if (options.v4SigningEnabled) {
            List<SignatureAlgorithm> sigAlgos =
                    V4SchemeSigner.getSuggestedSignatureAlgorithms(
                            publicKey,
                            options.minSdkVersion,
                            /* apkSigningBlockPaddingSupported= */ true,
                            /* deterministicDsaSigning= */ false);
            ApkSigningBlockUtils.SignerConfig innerV4 =
                    buildBlockSignerConfig(
                            keyConfig,
                            certificates,
                            sigAlgos,
                            /* minSdkVersion= */ 0,
                            /* maxSdkVersion= */ 0);
            v4Config = new V4SchemeSigner.SignerConfig(List.of(innerV4), /* v3Config= */ null);
        }

        return new FastSignerState(v2Config, v3Config, v4Config, new ArrayList<>(digestAlgorithms));
    }

    private static Archive createV1Archive(Options options, ExecutorService pool) throws Exception {
        List<X509Certificate> certificates = new ArrayList<>();
        PrivateKey privateKey = loadPrivateKey(options, certificates);
        SignedApkOptions.Builder builder =
                new SignedApkOptions.Builder()
                        .setName("CERT")
                        .setPrivateKey(privateKey)
                        .setCertificates(certificates)
                        .setExecutor(createExecutor(pool))
                        .setV1Enabled(options.v1SigningEnabled)
                        .setV2Enabled(options.v2SigningEnabled)
                        .setV3Enabled(options.v3SigningEnabled)
                        .setV4Enabled(options.v4SigningEnabled)
                        .setMinSdkVersion(options.minSdkVersion);
        if (options.v4SigningEnabled) {
            builder.setV4Output(idsigPath(options).toFile());
        }
        return new SignedApk(options.output.toFile(), builder.build());
    }

    private static String jcaDigestAlgorithm(ContentDigestAlgorithm algo) {
        return algo == ContentDigestAlgorithm.CHUNKED_SHA512 ? "SHA-512" : "SHA-256";
    }

    private static int digestSizeBytes(ContentDigestAlgorithm algo) {
        return algo == ContentDigestAlgorithm.CHUNKED_SHA512 ? 64 : SHA256_DIGEST_SIZE;
    }

    private static int getChunkCount(long size) {
        return (int) ((size + V2_CHUNK_SIZE - 1) / V2_CHUNK_SIZE);
    }

    private static void setIntLE(int value, byte[] out, int offset) {
        out[offset] = (byte) (value & 0xff);
        out[offset + 1] = (byte) ((value >>> 8) & 0xff);
        out[offset + 2] = (byte) ((value >>> 16) & 0xff);
        out[offset + 3] = (byte) ((value >>> 24) & 0xff);
    }

    private static void hashSectionChunks(
            ByteBuffer section,
            int startChunkIndex,
            List<ContentDigestAlgorithm> algos,
            byte[][] chunkDigestsByAlgo)
            throws Exception {
        int basePos = section.position();
        int size = section.remaining();
        int chunks = getChunkCount(size);
        byte[] header = new byte[5];
        header[0] = (byte) 0xa5;
        for (int a = 0; a < algos.size(); a++) {
            ContentDigestAlgorithm algo = algos.get(a);
            MessageDigest md = MessageDigest.getInstance(jcaDigestAlgorithm(algo));
            int digestSize = digestSizeBytes(algo);
            byte[] out = chunkDigestsByAlgo[a];
            for (int c = 0; c < chunks; c++) {
                int offset = c * V2_CHUNK_SIZE;
                int chunkLen = Math.min(V2_CHUNK_SIZE, size - offset);
                setIntLE(chunkLen, header, 1);
                md.update(header, 0, 5);
                int start = basePos + offset;
                ByteBuffer slice = section.duplicate();
                slice.position(start).limit(start + chunkLen);
                md.update(slice);
                md.digest(out, 5 + (startChunkIndex + c) * digestSize, digestSize);
            }
        }
    }

    private static void hashPayloadChunks(
            ByteBuffer payloadMap,
            long payloadSize,
            int payloadChunks,
            AtomicInteger nextChunk,
            List<ContentDigestAlgorithm> algos,
            int[] digestSizes,
            byte[][] chunkDigestsByAlgo,
            byte[] payloadLeafHashes)
            throws Exception {
        ByteBuffer localPayload = payloadMap.duplicate();
        int numAlgos = algos.size();
        MessageDigest[] v2Mds = new MessageDigest[numAlgos];
        for (int a = 0; a < numAlgos; a++) {
            v2Mds[a] = MessageDigest.getInstance(jcaDigestAlgorithm(algos.get(a)));
        }
        MessageDigest v4Md =
                payloadLeafHashes != null ? MessageDigest.getInstance("SHA-256") : null;
        byte[] chunkHeader = new byte[5];
        chunkHeader[0] = (byte) 0xa5;

        int chunkIndex;
        while ((chunkIndex = nextChunk.getAndIncrement()) < payloadChunks) {
            long chunkOffset = (long) chunkIndex * V2_CHUNK_SIZE;
            int chunkLen = (int) Math.min(V2_CHUNK_SIZE, payloadSize - chunkOffset);
            setIntLE(chunkLen, chunkHeader, 1);
            for (int a = 0; a < numAlgos; a++) {
                v2Mds[a].update(chunkHeader, 0, 5);
            }

            int startPos = (int) chunkOffset;
            if (v4Md != null) {
                int fullPages = chunkLen / V4_PAGE_SIZE;
                int basePageIdx = chunkIndex * (V2_CHUNK_SIZE / V4_PAGE_SIZE);
                for (int p = 0; p < fullPages; p++) {
                    int pageStart = startPos + p * V4_PAGE_SIZE;
                    localPayload.limit(pageStart + V4_PAGE_SIZE).position(pageStart);
                    for (int a = 0; a < numAlgos; a++) {
                        v2Mds[a].update(localPayload);
                        localPayload.position(pageStart);
                    }
                    v4Md.update(localPayload);
                    v4Md.digest(
                            payloadLeafHashes,
                            (basePageIdx + p) * SHA256_DIGEST_SIZE,
                            SHA256_DIGEST_SIZE);
                }
                int rem = chunkLen - fullPages * V4_PAGE_SIZE;
                if (rem > 0) {
                    int tailStart = startPos + fullPages * V4_PAGE_SIZE;
                    localPayload.limit(tailStart + rem).position(tailStart);
                    for (int a = 0; a < numAlgos; a++) {
                        v2Mds[a].update(localPayload);
                        localPayload.position(tailStart);
                    }
                }
            } else {
                localPayload.limit(startPos + chunkLen).position(startPos);
                for (int a = 0; a < numAlgos; a++) {
                    v2Mds[a].update(localPayload);
                    localPayload.position(startPos);
                }
            }

            for (int a = 0; a < numAlgos; a++) {
                int digestSize = digestSizes[a];
                v2Mds[a].digest(chunkDigestsByAlgo[a], 5 + chunkIndex * digestSize, digestSize);
            }
        }
    }

    private static int[] calculateVerityLevelOffsets(long dataSize) {
        List<Long> levelSizes = new ArrayList<>();
        while (true) {
            long chunkCount = (dataSize + V4_PAGE_SIZE - 1) / V4_PAGE_SIZE;
            long levelDataSize = chunkCount * SHA256_DIGEST_SIZE;
            long size = V4_PAGE_SIZE * ((levelDataSize + V4_PAGE_SIZE - 1) / V4_PAGE_SIZE);
            levelSizes.add(size);
            if (levelDataSize <= V4_PAGE_SIZE) {
                break;
            }
            dataSize = levelDataSize;
        }
        int numLevels = levelSizes.size();
        int[] levelOffset = new int[numLevels + 1];
        for (int i = 0; i < numLevels; i++) {
            levelOffset[i + 1] =
                    levelOffset[i] + Math.toIntExact(levelSizes.get(numLevels - i - 1));
        }
        return levelOffset;
    }

    /**
     * Performs single-pass zero-copy v2/v3 + v4 signing directly over a read-only {@link
     * MappedByteBuffer}.
     *
     * <p>Unlike {@code DefaultApkSignerEngine}, which reads the entire APK twice (once in {@code
     * ChunkSupplier} for v2/v3 and once in {@code VerityTreeBuilder} for v4) and allocates {@code 2
     * x APK_SIZE} of heap buffers via {@code DataSource.copyTo}, this hashes the 1 MiB v2/v3 chunks
     * and 4 KiB v4 verity leaves of {@code payload} (99.95% of the APK) together in a single
     * parallel pass over direct {@link MappedByteBuffer} slices while each 4 KiB page is hot in L1
     * cache, then finishes the ~320 KiB tail once the v2/v3 APK Signing Block is generated.
     */
    @SuppressWarnings("unchecked")
    private static void signV2AndV4SinglePass(
            Path outputPath,
            ZipInfo zipInfo,
            FastSignerState signerState,
            Options options,
            ExecutorService pool)
            throws Exception {
        long payloadSize = zipInfo.payload.size();
        long cdOffset = zipInfo.cd.first;
        int cdSize = Math.toIntExact(zipInfo.cd.size());
        int eocdSize = Math.toIntExact(zipInfo.eocd.size());
        int payloadChunks = getChunkCount(payloadSize);
        int cdChunks = getChunkCount(cdSize);
        int eocdChunks = getChunkCount(eocdSize);
        int totalChunks = payloadChunks + cdChunks + eocdChunks;
        boolean v4Enabled = options.v4SigningEnabled;

        List<ContentDigestAlgorithm> algos = signerState.contentDigestAlgorithms;
        int numAlgos = algos.size();
        byte[][] chunkDigestsByAlgo = new byte[numAlgos][];
        int[] digestSizes = new int[numAlgos];
        for (int a = 0; a < numAlgos; a++) {
            int digestSize = digestSizeBytes(algos.get(a));
            digestSizes[a] = digestSize;
            byte[] buf = new byte[5 + totalChunks * digestSize];
            buf[0] = 0x5a;
            setIntLE(totalChunks, buf, 1);
            chunkDigestsByAlgo[a] = buf;
        }

        int numFullPayloadPages = v4Enabled ? Math.toIntExact(payloadSize / V4_PAGE_SIZE) : 0;
        byte[] payloadLeafHashes =
                v4Enabled ? new byte[numFullPayloadPages * SHA256_DIGEST_SIZE] : null;

        byte[] signingBlock;
        byte[] tailBytes = new byte[cdSize + eocdSize];
        int payloadRem = (int) (payloadSize % V4_PAGE_SIZE);
        byte[] payloadTailBytes = v4Enabled ? new byte[payloadRem] : null;
        Map<Integer, byte[]> apkDigestsForV4 = new HashMap<>();

        try (FileChannel ch =
                FileChannel.open(outputPath, StandardOpenOption.READ, StandardOpenOption.WRITE)) {
            MappedByteBuffer payloadMap =
                    ch.map(FileChannel.MapMode.READ_ONLY, zipInfo.payload.first, payloadSize);

            AtomicInteger nextChunk = new AtomicInteger(0);
            int workerCount = Math.min(MAX_THREADS, Math.max(1, payloadChunks));
            List<Future<?>> futures = new ArrayList<>(workerCount);
            for (int w = 0; w < workerCount; w++) {
                futures.add(
                        pool.submit(
                                () -> {
                                    hashPayloadChunks(
                                            payloadMap,
                                            payloadSize,
                                            payloadChunks,
                                            nextChunk,
                                            algos,
                                            digestSizes,
                                            chunkDigestsByAlgo,
                                            payloadLeafHashes);
                                    return null;
                                }));
            }

            // Read Central Directory and EOCD while worker threads hash payload.
            ByteBuffer tailBuf = ByteBuffer.wrap(tailBytes);
            while (tailBuf.hasRemaining()) {
                if (ch.read(tailBuf, cdOffset + tailBuf.position()) < 0) {
                    throw new IOException("Unexpected EOF reading central directory");
                }
            }
            if (payloadTailBytes != null && payloadRem > 0) {
                ByteBuffer tailSlice = payloadMap.duplicate();
                tailSlice.position(numFullPayloadPages * V4_PAGE_SIZE).limit((int) payloadSize);
                tailSlice.get(payloadTailBytes);
            }

            ByteBuffer cdSlice = ByteBuffer.wrap(tailBytes, 0, cdSize);
            ByteBuffer eocdSlice =
                    ByteBuffer.wrap(tailBytes, cdSize, eocdSize)
                            .slice()
                            .order(ByteOrder.LITTLE_ENDIAN);
            ZipUtils.setZipEocdCentralDirectoryOffset(eocdSlice, payloadSize);

            hashSectionChunks(cdSlice, payloadChunks, algos, chunkDigestsByAlgo);
            hashSectionChunks(eocdSlice, payloadChunks + cdChunks, algos, chunkDigestsByAlgo);

            for (Future<?> future : futures) {
                future.get();
            }

            Map<ContentDigestAlgorithm, byte[]> contentDigests =
                    new EnumMap<>(ContentDigestAlgorithm.class);
            for (int a = 0; a < numAlgos; a++) {
                ContentDigestAlgorithm algo = algos.get(a);
                MessageDigest md = MessageDigest.getInstance(jcaDigestAlgorithm(algo));
                contentDigests.put(algo, md.digest(chunkDigestsByAlgo[a]));
            }

            List<Pair<byte[], Integer>> schemeBlocks = new ArrayList<>(2);
            byte[] bestDigestForV4 =
                    v4Enabled ? ApkSigningBlockUtils.pickBestDigestForV4(contentDigests) : null;
            if (options.v2SigningEnabled) {
                Pair<byte[], Integer> v2Block =
                        (Pair<byte[], Integer>)
                                GENERATE_V2_BLOCK_METHOD.invoke(
                                        null,
                                        List.of(signerState.v2Config),
                                        contentDigests,
                                        options.v3SigningEnabled,
                                        /* additionalAttributes= */ null);
                schemeBlocks.add(v2Block);
                if (bestDigestForV4 != null) {
                    apkDigestsForV4.put(
                            ApkSigningBlockUtils.VERSION_APK_SIGNATURE_SCHEME_V2, bestDigestForV4);
                }
            }
            if (options.v3SigningEnabled) {
                V3SchemeSigner v3Signer =
                        new V3SchemeSigner.Builder(
                                        /* beforeCentralDir= */ null,
                                        /* centralDir= */ null,
                                        /* eocd= */ null,
                                        List.of(signerState.v3Config))
                                .setBlockId(V3SchemeSigner.APK_SIGNATURE_SCHEME_V3_BLOCK_ID)
                                .build();
                Pair<byte[], Integer> v3Block =
                        (Pair<byte[], Integer>)
                                GENERATE_V3_BLOCK_METHOD.invoke(v3Signer, contentDigests);
                schemeBlocks.add(v3Block);
                if (bestDigestForV4 != null) {
                    apkDigestsForV4.put(
                            ApkSigningBlockUtils.VERSION_APK_SIGNATURE_SCHEME_V3, bestDigestForV4);
                }
            }

            signingBlock = ApkSigningBlockUtils.generateApkSigningBlock(schemeBlocks);
            ZipUtils.setZipEocdCentralDirectoryOffset(eocdSlice, cdOffset + signingBlock.length);

            ch.position(cdOffset);
            ch.write(ByteBuffer.wrap(signingBlock));
            ch.write(ByteBuffer.wrap(tailBytes));
        }

        if (v4Enabled) {
            long totalApkSize = payloadSize + signingBlock.length + tailBytes.length;
            int[] levelOffset = calculateVerityLevelOffsets(totalApkSize);
            byte[] tree = new byte[levelOffset[levelOffset.length - 1]];
            int leafLevelOffset = levelOffset[levelOffset.length - 2];
            System.arraycopy(payloadLeafHashes, 0, tree, leafLevelOffset, payloadLeafHashes.length);

            int remBytes = payloadRem + signingBlock.length + tailBytes.length;
            int remPages = (remBytes + V4_PAGE_SIZE - 1) / V4_PAGE_SIZE;
            byte[] remBuf = new byte[remPages * V4_PAGE_SIZE];
            if (payloadRem > 0) {
                System.arraycopy(payloadTailBytes, 0, remBuf, 0, payloadRem);
            }
            System.arraycopy(signingBlock, 0, remBuf, payloadRem, signingBlock.length);
            System.arraycopy(
                    tailBytes, 0, remBuf, payloadRem + signingBlock.length, tailBytes.length);

            MessageDigest md = MessageDigest.getInstance("SHA-256");
            int dstPos = leafLevelOffset + payloadLeafHashes.length;
            for (int p = 0; p < remPages; p++) {
                md.update(remBuf, p * V4_PAGE_SIZE, V4_PAGE_SIZE);
                md.digest(tree, dstPos, SHA256_DIGEST_SIZE);
                dstPos += SHA256_DIGEST_SIZE;
            }

            for (int level = levelOffset.length - 3; level >= 0; level--) {
                int srcStart = levelOffset[level + 1];
                int srcEnd = levelOffset[level + 2];
                int outPos = levelOffset[level];
                for (int pos = srcStart; pos < srcEnd; pos += V4_PAGE_SIZE) {
                    md.update(tree, pos, V4_PAGE_SIZE);
                    md.digest(tree, outPos, SHA256_DIGEST_SIZE);
                    outPos += SHA256_DIGEST_SIZE;
                }
            }

            md.update(tree, 0, V4_PAGE_SIZE);
            byte[] rootHash = md.digest();

            V4Signature.HashingInfo hashingInfo =
                    HASHING_INFO_CTOR.newInstance(
                            V4Signature.HASHING_ALGORITHM_SHA256,
                            V4Signature.LOG2_BLOCK_SIZE_4096_BYTES,
                            /* salt= */ null,
                            rootHash);
            V4Signature signature =
                    (V4Signature)
                            GENERATE_V4_SIGNATURE_METHOD.invoke(
                                    null,
                                    signerState.v4Config,
                                    hashingInfo,
                                    apkDigestsForV4,
                                    /* additionalData= */ null,
                                    totalApkSize);

            try (OutputStream out =
                    new BufferedOutputStream(Files.newOutputStream(idsigPath(options)))) {
                signature.writeTo(out);
                byte[] treeLenBytes = new byte[4];
                setIntLE(tree.length, treeLenBytes, 0);
                out.write(treeLenBytes);
                out.write(tree);
            }
        }
    }

    private static Path idsigPath(Options options) {
        return options.output.resolveSibling(options.output.getFileName() + ".idsig");
    }

    private static void deleteOutputs(Options options) throws IOException {
        Files.deleteIfExists(options.output);
        Files.deleteIfExists(idsigPath(options));
    }

    private static void writeItems(Archive archive, Deque<Future<?>> items) throws Exception {
        // Poll rather than iterate so that each source's in-memory payload can
        // be garbage collected once it has been written.
        Future<?> itemFuture;
        while ((itemFuture = items.pollFirst()) != null) {
            Object item = itemFuture.get();
            if (item instanceof ZipSource) {
                archive.add((ZipSource) item);
            } else {
                archive.add((Source) item);
            }
        }
    }

    public static void main(String[] args) throws Exception {
        Options options = parseArgs(args);
        ExecutorService pool = Executors.newFixedThreadPool(MAX_THREADS);
        try {
            // zipflinger opens existing files for in-place modification.
            deleteOutputs(options);
            boolean fastSign =
                    options.keystore != null
                            && !options.v1SigningEnabled
                            && (options.v2SigningEnabled || options.v3SigningEnabled);
            Deque<Future<?>> items = parseSpec(options.spec, pool);
            Future<FastSignerState> signerFuture =
                    fastSign ? pool.submit(() -> createFastSignerState(options)) : null;

            if (fastSign) {
                ZipArchive archive = new ZipArchive(options.output);
                ZipInfo zipInfo;
                try {
                    writeItems(archive, items);
                    zipInfo = archive.closeWithInfo();
                } catch (Exception e) {
                    archive.close();
                    deleteOutputs(options);
                    throw e;
                }
                FastSignerState signerState = signerFuture.get();
                try {
                    signV2AndV4SinglePass(options.output, zipInfo, signerState, options, pool);
                } catch (Exception e) {
                    deleteOutputs(options);
                    throw e;
                }
            } else {
                try (Archive archive =
                        options.keystore == null
                                ? new ZipArchive(options.output)
                                : createV1Archive(options, pool)) {
                    writeItems(archive, items);
                } catch (Exception e) {
                    // Do not leave behind a partially written output.
                    deleteOutputs(options);
                    throw e;
                }
            }
        } finally {
            pool.shutdownNow();
        }
    }
}
