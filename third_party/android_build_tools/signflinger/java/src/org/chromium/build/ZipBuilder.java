// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import com.android.apksig.ApkSignerEngine;
import com.android.apksig.DefaultApkSignerEngine;
import com.android.apksig.KeyConfig;
import com.android.apksig.util.DataSource;
import com.android.apksig.util.DataSources;
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

import java.io.IOException;
import java.io.InputStream;
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
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.Deque;
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
 *
 * <p>See 3pp/3pp.py for why this runs on the JVM rather than as a GraalVM native-image.
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
    private static final ThreadLocal<ByteBuffer> DIRECT_BUF =
            ThreadLocal.withInitial(() -> ByteBuffer.allocateDirect(DIRECT_BUFFER_SIZE));

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

    private static DefaultApkSignerEngine createSignerEngine(Options options, ExecutorService pool)
            throws Exception {
        List<X509Certificate> certificates = new ArrayList<>();
        PrivateKey privateKey = loadPrivateKey(options, certificates);
        DefaultApkSignerEngine.SignerConfig signerConfig =
                new DefaultApkSignerEngine.SignerConfig.Builder(
                                "CERT", new KeyConfig.Jca(privateKey), certificates, false)
                        .build();
        DefaultApkSignerEngine engine =
                new DefaultApkSignerEngine.Builder(List.of(signerConfig), options.minSdkVersion)
                        .setV1SigningEnabled(options.v1SigningEnabled)
                        .setV2SigningEnabled(options.v2SigningEnabled)
                        .setV3SigningEnabled(options.v3SigningEnabled)
                        .setOtherSignersSignaturesPreserved(false)
                        .build();
        engine.setExecutor(createExecutor(pool));
        return engine;
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

    /**
     * Performs v2/v3 and v4 signing using read-only {@link MappedByteBuffer}s rather than {@link
     * SignedApk}'s {@code FileChannelDataSource}, which serializes all worker threads on {@code
     * synchronized (mChannel)} and copies chunks through temporary direct buffers.
     */
    @SuppressWarnings("deprecation")
    private static void signV2AndV4(
            Path outputPath, ZipInfo zipInfo, DefaultApkSignerEngine signer, Options options)
            throws Exception {
        if (options.v2SigningEnabled || options.v3SigningEnabled) {
            try (FileChannel ch =
                    FileChannel.open(
                            outputPath, StandardOpenOption.READ, StandardOpenOption.WRITE)) {
                MappedByteBuffer payloadMap =
                        ch.map(
                                FileChannel.MapMode.READ_ONLY,
                                zipInfo.payload.first,
                                zipInfo.payload.size());
                DataSource payloadDs = DataSources.asDataSource(payloadMap);

                int cdAndEocdSize = (int) (zipInfo.cd.size() + zipInfo.eocd.size());
                ByteBuffer tailBuf =
                        ByteBuffer.allocate(cdAndEocdSize).order(ByteOrder.LITTLE_ENDIAN);
                ch.read(tailBuf, zipInfo.cd.first);
                tailBuf.flip();

                DataSource tailDs = DataSources.asDataSource(tailBuf);
                DataSource cdDs = tailDs.slice(0, zipInfo.cd.size());
                DataSource eocdDs = tailDs.slice(zipInfo.cd.size(), zipInfo.eocd.size());

                ApkSignerEngine.OutputApkSigningBlockRequest req =
                        signer.outputZipSections(payloadDs, cdDs, eocdDs);
                req.done();
                byte[] signingBlock = req.getApkSigningBlock();

                tailBuf.putInt(cdAndEocdSize - 6, (int) (zipInfo.cd.first + signingBlock.length));
                ch.position(zipInfo.cd.first);
                ch.write(ByteBuffer.wrap(signingBlock));
                tailBuf.position(0);
                ch.write(tailBuf);
            }
        }
        if (options.v4SigningEnabled) {
            try (FileChannel ch = FileChannel.open(outputPath, StandardOpenOption.READ)) {
                MappedByteBuffer fullMap = ch.map(FileChannel.MapMode.READ_ONLY, 0, ch.size());
                signer.signV4(
                        DataSources.asDataSource(fullMap), idsigPath(options).toFile(), false);
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
            boolean fastSign = options.keystore != null && !options.v1SigningEnabled;
            Deque<Future<?>> items = parseSpec(options.spec, pool);
            Future<DefaultApkSignerEngine> signerFuture =
                    fastSign ? pool.submit(() -> createSignerEngine(options, pool)) : null;

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
                DefaultApkSignerEngine signer = signerFuture.get();
                try {
                    signV2AndV4(options.output, zipInfo, signer, options);
                } catch (Exception e) {
                    deleteOutputs(options);
                    throw e;
                } finally {
                    signer.close();
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
