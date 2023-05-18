import { load } from "js-yaml";

export function filterStepsBySafeKeys(yamlString, safeKeys) {
  const yamlObject = load(yamlString);

  const steps = yamlObject.steps;
  const filteredSteps = steps.filter((step) => safeKeys.includes(step.key));

  return filteredSteps;
}
