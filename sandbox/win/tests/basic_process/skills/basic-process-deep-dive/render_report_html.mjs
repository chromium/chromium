#!/usr/bin/env node
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import {pathToFileURL} from 'node:url';

function escapeHtml(value) {
  return value.replaceAll('&', '&amp;')
      .replaceAll('<', '&lt;')
      .replaceAll('>', '&gt;')
      .replaceAll('"', '&quot;');
}

function page(title, body) {
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${escapeHtml(title)}</title>
<style>
:root { color-scheme: light dark; }
body {
  box-sizing: border-box;
  max-width: 1440px;
  margin: 0 auto;
  padding: 2rem;
  font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
}
h1, h2, h3, h4 { line-height: 1.2; }
h1, h2 { border-bottom: 1px solid #8886; padding-bottom: .3em; }
a { color: #0969da; }
table {
  display: block;
  overflow-x: auto;
  border-collapse: collapse;
  margin: 1rem 0;
}
th, td { border: 1px solid #8887; padding: .35rem .6rem; vertical-align: top; }
th { background: #8882; }
code {
  border-radius: 4px;
  padding: .1em .3em;
  background: #8882;
  font-family: ui-monospace, "Cascadia Code", Consolas, monospace;
}
pre {
  overflow-x: auto;
  border: 1px solid #8885;
  border-radius: 6px;
  padding: .8rem;
  background: #8881;
}
pre code { padding: 0; background: transparent; }
blockquote { margin-left: 0; padding-left: 1rem; border-left: 4px solid #8886; }
@media print {
  body { max-width: none; padding: 0; font-size: 10pt; }
  a { color: inherit; text-decoration: none; }
  pre, table { break-inside: avoid; }
}
</style>
</head>
<body>
${body}
</body>
</html>
`;
}

function reportTitle(markdown, fallback) {
  const match = markdown.match(/^#\s+(.+)$/m);
  return match ? match[1].replaceAll('`', '') : fallback;
}

function writeIndex(directory, htmlFiles) {
  const evidenceExtensions = new Set(['.csv', '.txt']);
  const evidence =
      fs.readdirSync(directory)
          .filter(
              name => evidenceExtensions.has(path.extname(name).toLowerCase()))
          .sort((a, b) => a.localeCompare(b));
  const extraHtml =
      fs.readdirSync(directory)
          .filter(
              name => name.endsWith('.html') && name !== 'index.html' &&
                  !htmlFiles.includes(name))
          .sort((a, b) => a.localeCompare(b));

  const section = (heading, names) => names.length === 0 ?
      '' :
      `<h2>${heading}</h2><ul>${
          names
              .map(
                  name => `<li><a href="${encodeURIComponent(name)}">${
                      escapeHtml(name)}</a></li>`)
              .join('')}</ul>`;
  const body = `<h1>Basic Process report artifacts</h1>
${section('Reports', htmlFiles)}
${section('Interactive views', extraHtml)}
${section('Evidence', evidence)}`;
  fs.writeFileSync(
      path.join(directory, 'index.html'),
      page('Basic Process report artifacts', body));
}

const inputs = process.argv.slice(2).map(input => path.resolve(input));
if (inputs.length === 0) {
  console.error('usage: render_report_html.mjs <report.md> [<report.md> ...]');
  process.exit(2);
}

const markedPath = path.join(
    process.cwd(), 'third_party', 'devtools-frontend', 'src', 'front_end',
    'third_party', 'marked', 'package', 'lib', 'marked.esm.js');
if (!fs.existsSync(markedPath)) {
  throw new Error(`Run from the Chromium source root; missing ${markedPath}`);
}
const {marked} = await import(pathToFileURL(markedPath).href);
marked.setOptions({gfm: true});

const outputNames = [];
for (const input of inputs) {
  if (path.extname(input).toLowerCase() !== '.md') {
    throw new Error(`Expected a Markdown input: ${input}`);
  }
  const markdown = fs.readFileSync(input, 'utf8');
  const output = input.slice(0, -3) + '.html';
  fs.writeFileSync(
      output,
      page(
          reportTitle(markdown, path.basename(input)), marked.parse(markdown)));
  outputNames.push(path.basename(output));
  console.log(`wrote ${output}`);
}

if (inputs.length > 1) {
  const directories = new Set(inputs.map(input => path.dirname(input)));
  if (directories.size !== 1) {
    throw new Error('All reports must share a directory to create index.html');
  }
  const directory = path.dirname(inputs[0]);
  writeIndex(directory, outputNames.sort((a, b) => a.localeCompare(b)));
  console.log(`wrote ${path.join(directory, 'index.html')}`);
}
