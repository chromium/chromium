// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @file In this file, we find all CDT event and category names
 * and produce a lookup table for `devtools`.
 */

import fs from 'fs';
import path from 'path';
import startCase from 'lodash/startCase';
import { EventHandlerType } from "@replayio/protocol";


type EventDefinition = {
  label: string;
  type: EventHandlerType;
  eventTargets?: string[];
};


const ChromiumSrcDir = path.resolve(__dirname, '..');
const CdtDir = path.resolve(ChromiumSrcDir, 'third_party/devtools-frontend');
const DOMDebuggerModelPath = path.resolve(
  CdtDir,
  'src/front_end/core/sdk/DOMDebuggerModel.ts'
);

/**
 * Read event descriptions from CDT's `DOMDebuggerManager.ts`.
 * @see https://chromium.googlesource.com/devtools/devtools-frontend/+/3a80260722c77d984a637b923cad4883857e57dc/front_end/core/sdk/DOMDebuggerModel.ts#L758
 */
const cdtEventsSrc = fs.readFileSync(DOMDebuggerModelPath).toString();

/** ###########################################################################
 * parse CDT events
 * ##########################################################################*/

const res = cdtEventsSrc.matchAll(
  /i18nString\(UIStrings\.(.+?)\),\s*?(\[[^\]]*\])(?:,\s*?(\[[^\]]*\]))?.*?/gm
);

const cdtCategoriesRaw = [...res];

// sanity checks: if category count suddenly jumped a lot, there might be a bug
const good = cdtCategoriesRaw.length > 20 && cdtCategoriesRaw.length < 35;
console.log(`Found ${cdtCategoriesRaw.length} CDT source categories (w/ duplicates) ${good ? '✅' : '❌'}`);

if (!good) {
  console.log(' ', cdtCategoriesRaw.join('\n  '));
  die();
}


const cdtCategoryOverrides: { [key: string]: string } = {
  Xhr: 'XHR',
  'Drag Drop': 'Drag and Drop',
  'Dom Mutation': 'DOM Mutation'
};

/**
 * Prettier CDT category names
 */
function prettyCDTCategory(category: string) {
  category = startCase(category);
  return cdtCategoryOverrides[category] || category;
}

let cdtEvents = cdtCategoriesRaw.map(cat => {
  const category = prettyCDTCategory(cat[1]);
  const events = eval(cat[2]);
  const eventTargets = (cat[3] && eval(cat[3]) || ['*']) as string[];
  return {
    // attempt basic normalization
    category,
    eventTargets: eventTargets.map(t => t.toLowerCase()),
    events: events.map((x: string): EventDefinition => ({
        type: x,
        label: x,
      })
    ) as EventDefinition[]
  };
});


console.log('Done.');
console.groupEnd();


const code = genTs();
const codeFile = __dirname + "/event-names-code.gen.ts";
fs.writeFileSync(codeFile, code);
console.log(`\n=== Generated code has been written to:\n  ${codeFile}\n`);


/** ###########################################################################
 * generate event table (TS)
 * ##########################################################################*/

function genTs() {
  const Indent = ' '.repeat(4);
  return `\
// <GENERATED CODE. DO NOT EDIT.>
// NOTE: This table is generated via \`ts-node $CHROMIUM_DIR/src/replay-scripts/gen-event-names.ts\`
${JSON.stringify(cdtEvents, null, 2)}
// </GENERATED CODE. DO NOT EDIT.>`
  .split('\n')
  .map(s => Indent + s)
  .join('\n');
}



/** ###########################################################################
 * utils
 * ##########################################################################*/

function die() {
  // NOTE: in some terminals, this produces a nice, red error indicator
  process.exit(-1);
}
