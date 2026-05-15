# MAGI Example Configurations

This file contains full JSON examples for configurations used in the MAGI
protocol.

## `project.magi.json`

```json
{
  "$schema": "./magi_schema.json#definitions/ProjectSpec",
  "task_type": "IMPLEMENTATION",
  "goal": "A one-sentence summary of the fix/feature.",
  "target_files": ["Absolute paths to the files that must be modified."],
  "anti_goals": ["What should explicitly NOT be changed."],
  "edge_cases": ["Specific warnings from logs or code context."],
  "build_targets": ["//remoting/host:host"],
  "next_stage": "SCAFFOLDING",
  "paranoia_mode": false,
  "auditability_level": "NORMAL",
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
    "[Merged keys from selected personas]": false
  },
  "iteration": 1,
  "stall_count": 0,
  "active_constraints": [],
  "resolved_constraints": [],
  "unlisted_issues_found": [],
  "implementors": ["[Selected Implementors]"],
  "reviewers": ["[Selected Reviewers]"],
  "next_stage": "[Determined by task type]",
  "review_mode": "[SUPERVISOR/CONSENSUS]",
  "state_transport": "[FILE_IO/EPHEMERAL/EPHEMERAL_WITH_LOGS]"
}
```
