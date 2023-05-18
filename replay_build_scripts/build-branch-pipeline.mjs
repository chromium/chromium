import { load } from "js-yaml";
import { promises as fs } from "fs";

import { spawnChecked } from "./common.mjs";

async function getBuildkiteMetadata(key) {
  // buildkite-agent meta-data get <key>
  const result = spawnChecked("buildkite-agent", ["meta-data", "get", key]);
  const output = result.stdout.toString().trim();

  return output;
}

async function getConfiguredPlatformArchitectures() {
  const output = await getBuildkiteMetadata("platform-architecture");
  const platformArchitectures = output.split("\n");

  return platformArchitectures;
}

async function main() {
  const yamlString = await fs.readFile(".buildkite/pipeline.yml", "utf8");
  const parsedYaml = load(yamlString);
  const targetPlatformArchitectures =
    await getConfiguredPlatformArchitectures();
  const buildStepKeys = targetPlatformArchitectures.map(
    (platformArchitecture) => `build-chromium-${platformArchitecture}`
  );

  const yamlWithoutExcludedBuildSteps = parsedYaml.steps.filter((step) => {
    if (
      (step.key &&
        step.key.startsWith("build-") &&
        !buildStepKeys.includes(step.key)) ||
      buildStepKeys.includes(step.depends_on)
    ) {
      return false;
    }

    return true;
  });

  const buf = Buffer.from(JSON.stringify(yamlWithoutExcludedBuildSteps));

  spawnChecked("buildkite-agent", ["pipeline", "upload"], {
    input: buf,
  });
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
