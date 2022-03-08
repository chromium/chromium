'use strict';

// Filters an array of FontMetadata by font names. Used to reduce down
// the size of test data.
function filterFonts(fonts, filter) {
  const filteredFont = [];
  for (const font of fonts) {
    if (filter.includes(font.postscriptName)) {
      filteredFont.push(font);
    }
  }
  return filteredFont;
}

async function parseFontData(fontBlob) {
  const fontInfo = {
    errors: [],
    numTables: 0,
  };
  const versionTag = await getTag(fontBlob, 0);

  fontInfo.version = sfntVersionInfo(versionTag);
  if (fontInfo.version === 'UNKNOWN') {
    fontInfo.errors.push(`versionTag: "${versionTag}"`);
  }

  const numTables = await getUint16(fontBlob, 4);
  [fontInfo.tables, fontInfo.tableMeta] =
      await getTableData(fontBlob, numTables);

  return fontInfo;
}

async function getTableData(fontBlob, numTables) {
  const dataMap = new Map();
  const metaMap = new Map();
  let blobOffset = 12;

  for (let i = 0; i < numTables; i++) {
    const tag = await getTag(fontBlob, blobOffset);
    const checksum = await getUint32(fontBlob, blobOffset + 4);
    const offset = await getUint32(fontBlob, blobOffset + 8);
    const size = await getUint32(fontBlob, blobOffset + 12);
    const tableBlob = fontBlob.slice(offset, offset + size);
    dataMap.set(tag, tableBlob);
    metaMap.set(tag, {checksum, offset, size});
    blobOffset += 16;
  }

  return [dataMap, metaMap];
}

function sfntVersionInfo(version) {
  // Spec: https://docs.microsoft.com/en-us/typography/opentype/spec/otff#organization-of-an-opentype-font
  switch (version) {
  case '\x00\x01\x00\x00':
  case 'true':
  case 'typ1':
    return 'truetype';
  case 'OTTO':
    return 'cff';
  default:
    return 'UNKNOWN';
  }
}

async function getTag(blob, offset) {
  return (new TextDecoder).decode(
      await blob.slice(offset, offset + 4).arrayBuffer());
}

async function getUint16(blob, offset) {
  const slice = blob.slice(offset, offset + 2);
  const buf = await slice.arrayBuffer();
  const dataView = new DataView(buf);
  return dataView.getUint16(0);
}

async function getUint32(blob, offset) {
  const slice = blob.slice(offset, offset + 4);
  const buf = await slice.arrayBuffer();
  const dataView = new DataView(buf);
  return dataView.getUint32(0);
}

function promiseDocumentReady() {
  return new Promise(resolve => {
    if (document.readyState === 'complete') {
      resolve();
    }
    window.addEventListener('load', () => {
      resolve();
    }, {once: true});
  });
}

function isPlatformSupported() {
  if (navigator.platform.indexOf('Mac') !== -1 ||
      navigator.platform.indexOf('Win') !== -1 ||
      navigator.platform.indexOf('Linux') !== -1) {
    return true;
  }
  return false;
}

async function simulateUserActivation() {
  await promiseDocumentReady();
  return new Promise(resolve => {
    const button = document.createElement('button');
    button.textContent = 'Click to enumerate fonts';
    button.style.fontSize = '40px';
    button.onclick = () => {
      document.body.removeChild(button);
      resolve();
    };
    document.body.appendChild(button);
    test_driver.click(button);
  });
}

function font_access_test(test_function, name, properties) {
  return promise_test(async (t) => {
    if (!isPlatformSupported()) {
      await promise_rejects_dom(
          t, 'NotSupportedError', navigator.fonts.query());
      return;
    }
    await test_driver.set_permission({name: 'font-access'}, 'granted');
    await simulateUserActivation();
    await test_function(t, name, properties);
  });
}
