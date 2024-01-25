import * as fs from "fs";
import * as eslint from "eslint";

import * as path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const lintFile =
  process.argv[2] ||
  path.join(
    __dirname,
    "../third_party/blink/renderer/bindings/core/v8/record_replay_interface.cc",
  );

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

function getNamedTextBlock(
  text /*: string*/,
  start /*: number*/,
  end /*: number*/,
) {
  const lines = text.split("\n");
  const name = lines[start - 1].split(" ")[2];
  const block_lines = lines.slice(start, end);
  return { name, text: "\n".repeat(start) + block_lines.join("\n") };
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

async function lintScript({ name, text } /*: { name: string, text: string }*/) {
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

  let results = [
    {
      filePath: `${lintFile} (${name})`,
      messages,
      ...calculateStatsPerFile(messages),
    },
  ];

  const formatter = await engine.loadFormatter();
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

async function main() {
  const replayText = fs.readFileSync(lintFile, "utf8");

  const regex = new RegExp("//js", "g");
  const endRegex = new RegExp('\\)""""', "g");

  const lineNumbers = findMatches(replayText, regex);
  const endLineNumbers = findMatches(replayText, endRegex);

  const textBlocks = lineNumbers.map((lineNumber, index) =>
    getNamedTextBlock(replayText, lineNumber, endLineNumbers[index]),
  );

  let errorCount = 0;
  let warningCount = 0;
  for (const block of textBlocks) {
    const { errorCount: blockErrorCount, warningCount: blockWarningCount } =
      await lintScript(block);
    errorCount += blockErrorCount;
    warningCount += blockWarningCount;
  }

  console.log(`Total counts: ${errorCount} errors, ${warningCount} warnings`);
  if (errorCount > 0) {
    process.exit(1);
  }
}

main();
