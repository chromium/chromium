# MAGI Example Configurations

This file contains full JSON examples for configurations used in the MAGI
protocol.

## `project.magi.json`

```json
{
  "$schema": "./magi_schema.json#definitions/ProjectSpec",
  "task_type": "IMPLEMENTATION",
  "execution_path": "RIGOR_PATH",
  "complexity_level": "MEDIUM",
  "goal": "A one-sentence summary of the fix/feature.",
  "target_files": ["Absolute paths to the files that must be modified."],
  "anti_goals": ["What should explicitly NOT be changed."],
  "edge_cases": ["Specific warnings from logs or code context."],
  "build_targets": ["//remoting/host:host"],
  "next_stage": "SCAFFOLDING",
  "paranoia_mode": false,
  "auditability_level": "NORMAL",
  "context_resolved": true,
  "approach_confirmed": true,
  "ambiguity_level": "LOW",
  "ambiguity_rationale": "Direct request to modify a specific file.",
  "environment": {
    "repo_type": "CHROMIUM",
    "vcs": "JJ",
    "harness": "JETSKI",
    "output_directory": "out/Default"
  }
}
```

## `state_block.magi.json`

```json
{
  "$schema": "./magi_schema.json#definitions/StateBlock",
  "checklist": {
    "[Merged keys from selected rulesets]": false
  },
  "iteration": 1,
  "oscillation_detected": false,
  "conflict_report": [],
  "active_constraints": [],
  "resolved_constraints": [],
  "ignored_constraints": [],
  "unlisted_issues_found": [],
  "next_stage": "[Determined by task type]",
  "state_transport": "[FILE_IO/EPHEMERAL/EPHEMERAL_WITH_LOGS]"
}
```
