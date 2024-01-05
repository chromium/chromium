import * as fs from 'fs';
import * as eslint from 'eslint';

import * as path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

let lintFile = process.argv[2] ||
    path.join(__dirname, "../third_party/blink/renderer/bindings/core/v8/record_replay_interface.cc");

let replayText = fs.readFileSync(lintFile, 'utf8');

let regex = new RegExp('//js', 'g');
let endRegex = new RegExp('\\)""""', 'g');
const linter = new eslint.Linter();

function findMatches(text/*: string*/, regex/*: RegExp*/) {
    let match;
    const matches/*: number[] */ = [];
    while ((match = regex.exec(text)) != null) {
        const res = text.substr(0, match.index).split('\n').length - 1;
        matches.push(res);
    }

    return matches
}

function getNamedTextBlock(text/*: string*/, start/*: number*/, end/*: number*/) {
    const lines = text.split('\n');
    const name = lines[start-1].split(' ')[2];
    const block_lines = lines.slice(start, end);
    return { name, text: "\n".repeat(start) + block_lines.join('\n') };
}

function lintScript({ name, text }/*: { name: string, text: string }*/) {
    const messages = linter.verify(text, {
        parserOptions: {
            ecmaVersion: 2023,
            sourceType: "module",
        },
        rules: {
            "no-undef": ["error"],
            "no-unused-vars": ["warn", {
              "argsIgnorePattern": "^_",
              "varsIgnorePattern": "^_",
              "caughtErrorsIgnorePattern": "^_",
            }],
        },
        env: {
            "browser": true
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
            InspectorUtils: true

        }
    });

    if (messages.length > 0) {
        console.log(`Script ${name}:\n`, messages)
    }

    return {
        errors: messages.filter(m => m.severity === 2).length,
        warnings: messages.filter(m => m.severity === 1).length,
    }
}

const lineNumbers = findMatches(replayText, regex)
const endLineNumbers = findMatches(replayText, endRegex)
// console.log('Lines with "//js":', lineNumbers);
// console.log('Lines with ")"""":', endLineNumbers);

const textBlocks = lineNumbers.map((lineNumber, index) => getNamedTextBlock(replayText, lineNumber, endLineNumbers[index]))

let errorCount = 0;
let warningCount = 0;
for (const block of textBlocks) {
  const { errors, warnings } = lintScript(block);
  errorCount += errors;
  warningCount += warnings;
}

console.log(`ESLint: ${errorCount} errors, ${warningCount} warnings`)
if (errorCount > 0) {
    process.exit(1)
}

