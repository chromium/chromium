import * as fs from "fs";
import * as eslint from "eslint";

import * as path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT_DIR = path.join(__dirname, "..");

const linter = new eslint.Linter();
const engine = new eslint.ESLint();

function findMatches(text /*: string*/, regex /*: RegExp*/) {
  let match;
  const matches /*: number[] */ = [];
  while ((match = regex.exec(text)) != null) {
    const res = text.substr(0, match.index).split("\n").length - 1;
    matches.push(res);
  }

  return matches;
}

function extractNamedScriptBlock(
  text /*: string*/,
  start /*: number*/,
  end /*: number*/
) {
  const lines = text.split("\n");
  const name = lines[start].split(" ")[2];

  // NOTE: Ignore the start and end lines.
  const block_lines = lines.slice(start + 1, end);
  // console.log("block_lines",start, end, block_lines[0], block_lines[1]);
  return { name, text: "\n".repeat(start + 1) + block_lines.join("\n") };
}

// this function from eslint (lib/eslint/flat-eslint.js)
function calculateStatsPerFile(messages) {
  const stat = {
    errorCount: 0,
    fatalErrorCount: 0,
    warningCount: 0,
    fixableErrorCount: 0,
    fixableWarningCount: 0,
  };

  for (let i = 0; i < messages.length; i++) {
    const message = messages[i];

    if (message.fatal || message.severity === 2) {
      stat.errorCount++;
      if (message.fatal) {
        stat.fatalErrorCount++;
      }
      if (message.fix) {
        stat.fixableErrorCount++;
      }
    } else {
      stat.warningCount++;
      if (message.fix) {
        stat.fixableWarningCount++;
      }
    }
  }
  return stat;
}

async function lintScript(
  fpath,
  { name, text } /*: { name: string, text: string }*/
) {
  const messages = linter.verify(text, {
    parserOptions: {
      ecmaVersion: 2023,
      sourceType: "module",
    },

    rules: {
      "no-undef": ["error"],
      "no-unused-vars": [
        "error",
        {
          argsIgnorePattern: "^_",
          varsIgnorePattern: "^_",
          caughtErrorsIgnorePattern: "^_",
        },
      ],
    },

    env: {
      browser: true,
    },
    globals: {
      __RECORD_REPLAY_ARGUMENTS__: true,
      __RECORD_REPLAY__: true,
      log: true,

      // browser globals
      CSSStyleValue: true,
      CSSStyleDeclaration: true,
      Element: true,
      Map: true,
      Node: true,
      window: true,
      URL: true,
      Set: true,
      location: true,

      // CDP
      InspectorUtils: true,
    },
  });

  // console.log("messages", fpath, JSON.stringify(messages, null, 2), text);

  let results = [
    {
      filePath: `${name ? `${name}: ` : ""} ${fpath}`,
      messages,
      ...calculateStatsPerFile(messages),
    },
  ];

  const formatter = await engine.loadFormatter("visualstudio");
  console.log(await formatter.format(results, {}));

  let errorCount = 0;
  let fatalErrorCount = 0;
  let warningCount = 0;

  for (const result of results) {
    errorCount += result.errorCount;
    fatalErrorCount += result.fatalErrorCount;
    warningCount += result.warningCount;
  }

  return { errorCount, fatalErrorCount, warningCount };
}

const AssetFiles = [
  "replay_command_handlers.js",
  "replay_sourcemap_handler.js",
];

let totalErrorCount = 0;
let totalWarningCount = 0;

async function main() {
  await lintFile(
    path.join(
      ROOT_DIR,
      "third_party/blink/renderer/bindings/core/v8/record_replay_interface.cc"
    ),
    /R""""\(/g,
    /\)""""/g
  );

  for (const jsFile of AssetFiles) {
    await lintFile(path.join(ROOT_DIR, "replay-assets/" + jsFile));
  }

  const bad = !!totalErrorCount;
  console.log(`\n${bad ? '❌' : '✅'} Final Result:\n  ${totalErrorCount} errors\n  ${totalWarningCount} warnings`);
  console.groupEnd();
  if (bad) {
    process.exit(1);
  }
}

async function lintFile(fpath, startRegex, endRegex) {
  const replayText = fs.readFileSync(fpath, "utf8");

  let jsTextBlocks;
  if (startRegex) {
    const lineNumbers = findMatches(replayText, startRegex);
    const endLineNumbers = findMatches(replayText, endRegex);
    if (lineNumbers?.length != endLineNumbers?.length) {
      throw new Error(
        `Lint failed in ${fpath} - start and end line numbers don't match: ${lineNumbers?.length} != ${endLineNumbers?.length}`
      );
    }
    // console.log("lintFile", lineNumbers?.length, endLineNumbers?.length);
    jsTextBlocks = lineNumbers.map((lineNumber, index) =>
      extractNamedScriptBlock(replayText, lineNumber, endLineNumbers[index])
    );

    if (!jsTextBlocks.length) {
      throw new Error(
        `Invalid regexes or file path. Could not find js text block in ${fpath}.`
      );
    }
  } else {
    jsTextBlocks = [{ text: replayText }];
  }

  let errorCount = 0;
  let warningCount = 0;
  console.group(`Linting ${jsTextBlocks.length} scripts in ${fpath}...:`);
  for (const jsTextBlock of jsTextBlocks) {
    const { errorCount: blockErrorCount, warningCount: blockWarningCount } =
      await lintScript(fpath, jsTextBlock);
    errorCount += blockErrorCount;
    warningCount += blockWarningCount;
  }

  console.log(`Stats: ${errorCount} errors, ${warningCount} warnings`);
  totalErrorCount += errorCount;
  totalWarningCount += warningCount;
  console.groupEnd();
}

main();