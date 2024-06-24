// Script which sets a handler for collecting source maps from scripts in the
// recording. Runs when recording/replaying if source map collection is enabled.
(() => {

// Avoid monkey patching.
const { fetch, URL, Error } = window;
const DateNow = Date.now;

const {
  log,
  warning,
  getRecordingId,
  sha256DigestHex,
  writeToRecordingDirectory,
  addRecordingEvent,
  addNewScriptHandler,
  getScriptSource,
  recordingDirectoryFileExists,
  readFromRecordingDirectory,
  getRecordingFilePath,
  RECORD_REPLAY_DISABLE_SOURCEMAP_CACHE,
} = __RECORD_REPLAY_ARGUMENTS__;

const fetchPromiseCache = {};

async function fetchText(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Fetching ${url} failed with status code ${response.status} (${response.statusText})`);
  }
  return await response.text();
}

// Provide a cache for urls, salted with the supplied hash.  Practically, this
// means if the script content changes at the url, we will re-download the resource.
async function fetchTextWithCache(url, hash) {
  const key = `${url}:${hash}`;
  if (fetchPromiseCache[key] && !RECORD_REPLAY_DISABLE_SOURCEMAP_CACHE) {
    // Return past or on-going work item.
    return fetchPromiseCache[key];
  }

  log(`[sourcemaps] Fetching sourcemap resource ${key}`);

  const resPromise = fetchText(url);
  fetchPromiseCache[key] = resPromise;
  return resPromise;
}

addNewScriptHandler(async (scriptId, sourceURL, relativeSourceMapURL) => {
  try {
  if (!relativeSourceMapURL || relativeSourceMapURL.startsWith("data:"))
    return;

  const recordingId = getRecordingId();
  if (!recordingId) {
    // The recording has been invalidated.
    return;
  }

  const urls = getSourceMapURLs(sourceURL, relativeSourceMapURL);
  if (!urls)
    return;

  const scriptSource = getScriptSource(scriptId);
  const generatedScriptHash = sha256DigestHex(scriptSource);

  const { sourceMapURL, sourceMapBaseURL } = urls;

  let sourceMap;
  try {
    sourceMap = await fetchTextWithCache(sourceMapURL, generatedScriptHash);
  } catch (err) {
    log(`[RuntimeError][sourcemaps] Failed to read sourcemap ${sourceMapURL}: ${err.message}`);
  }
  if (!sourceMap) {
    // Download failed or nothing there.
    return;
  }

  const id = generatedScriptHash;
  const name = `sourcemap-${id}.map`;
  const lookupName = `sourcemap-${id}.lookup`;

  let sources;
  if (recordingDirectoryFileExists(name) && recordingDirectoryFileExists(lookupName)) {
    try {
      sources = JSON.parse(readFromRecordingDirectory(lookupName));
    } catch (err) {
      log(`[RuntimeError][sourcemaps] Failed to load sourcemaps from file: ${lookupName} - ${err.message}`);
    }
  }

  if (!sources) {
    // Sources changed or did not exist.
    writeToRecordingDirectory(name, sourceMap);

    sources = collectUnresolvedSourceMapResources(sourceMap, sourceMapURL);
    writeToRecordingDirectory(lookupName, JSON.stringify(sources));
  }

  log(`[sourcemaps] Wrote sourcemap to file. Found ${sources.length} unresolved sources for "${sourceMapURL}". Downloading...`);

  addRecordingEvent(JSON.stringify({
    kind: "sourcemapAdded",
    path: getRecordingFilePath(name),
    recordingId,
    id,
    url: sourceMapURL,
    baseURL: sourceMapBaseURL,
    targetContentHash: `sha256:${generatedScriptHash}`,
    targetURLHash: sourceURL ? makeAPIHash(sourceURL) : undefined,
    targetMapURLHash: makeAPIHash(sourceMapURL),
    timestamp: DateNow(),
  }));

  for (const { offset, url } of sources) {
    let sourceContent;
    try {
      sourceContent = await fetchTextWithCache(url, generatedScriptHash);
    } catch (err) {
      log(`[RuntimeError][sourcemaps] Failed to read original source ${url}: ${err.message}`);
    }
    if (!sourceContent) {
      // Download failed or nothing there.
      continue;
    }
    const hash = sha256DigestHex(sourceContent);
    const name = `source-${hash}`;

    if (!recordingDirectoryFileExists(name)) {
      writeToRecordingDirectory(name, sourceContent);
    }
    addRecordingEvent(JSON.stringify({
      kind: "originalSourceAdded",
      path: getRecordingFilePath(name),
      recordingId,
      parentId: id,
      parentOffset: offset,
      timestamp: DateNow(),
    }));
  }
  log(`[sourcemaps] Finished downloading ${sources.length} sources for "${sourceMapURL}".`);
  } catch (err) {
    warning(`[RuntimeError][sourcemaps] Exception - ${err?.stack || err}`);
  }
});

function makeAPIHash(content) {
  assert(typeof content === "string");
  const digestHex = sha256DigestHex(content);
  return "sha256:" + digestHex;
}

function collectUnresolvedSourceMapResources(mapText, mapURL) {
  let obj;
  let sourceOffset = 0;

  function logError(msg) {
    log(`[RuntimeError][sourcemaps] ${msg} (${mapURL}:${sourceOffset})`);
  }

  try {
    obj = JSON.parse(mapText);
    if (typeof obj !== "object" || !obj) {
      return [];
    }
  } catch (err) {
    logError(`Exception parsing sourcemap JSON (${mapURL}): ${err?.message || err}`);
    return [];
  }

  const unresolvedSources = [];
  if (obj.version !== 3) {
    logError("Invalid sourcemap version: " + obj.version);
    return [];
  }

  if (obj.sources != null) {
    const { sourceRoot, sources, sourcesContent } = obj;

    if (Array.isArray(sources)) {
      for (let i = 0; i < sources.length; i++) {
        const offset = sourceOffset++;

        if (
          !Array.isArray(sourcesContent) ||
          typeof sourcesContent[i] !== "string"
        ) {
          let url = sources[i];
          if (typeof sourceRoot === "string" && sourceRoot) {
            url = sourceRoot.replace(/\/?/, "/") + url;
          }
          let sourceURL;
          try {
            sourceURL = new URL(url, mapURL).toString();
          } catch {
            logError("Unable to compute original source URL: " + url);
            continue;
          }

          unresolvedSources.push({
            offset,
            url: sourceURL,
          });
        }
      }
    } else {
      logError("Invalid sourcemap sources list");
    }
  }

  return unresolvedSources;
}

function assert(v, msg = "") {
  if (!v) {
    const m = `Assertion failed when handling command (${msg})`;
    log(`[RuntimeError] ${m} - ${Error().stack}`);
    throw new Error(m);
  }
}

function getSourceMapURLs(sourceURL, relativeSourceMapURL) {
  let sourceBaseURL;
  if (typeof sourceURL === "string" && isValidBaseURL(sourceURL)) {
    sourceBaseURL = sourceURL;
  } else if (window?.location?.href && isValidBaseURL(window?.location?.href)) {
    sourceBaseURL = window.location.href;
  }

  let sourceMapURL;
  try {
    sourceMapURL = new URL(relativeSourceMapURL, sourceBaseURL).toString();
  } catch (err) {
    log("Failed to process sourcemap url: " + err.message);
    return null;
  }

  // If the map was a data: URL or something along those lines, we want
  // to resolve paths in the map relative to the overall base.
  const sourceMapBaseURL =
    isValidBaseURL(sourceMapURL) ? sourceMapURL : sourceBaseURL;

  return { sourceMapURL, sourceMapBaseURL };
}

function isValidBaseURL(url) {
  try {
    new URL("", url);
    return true;
  } catch {
    return false;
  }
}

})();
