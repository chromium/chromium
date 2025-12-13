# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#!/usr/bin/env python3

import argparse
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
import sys
from urllib.parse import urlparse

# Data source embedded in the script
# Only includes documents EXTERNAL to the chromium/src repository.
DOCUMENT_MANIFEST = {
  "description": "Manifest of externally sourced documents cached for AI assistant context.",
  "remote_documents": [
    {
      "source_url": "https://gn.googlesource.com/gn/+/main/docs/style_guide.md",
      "cached_path": "style_guide.md",
      "subdir": "gn",
      "description": "Style guide for writing clean and maintainable BUILD.gn files."
    },
  ]
}

def parse_gob_url(url):
    """Parses a Git-on-Borg URL into repo URL, branch, and file path."""
    try:
        parts = url.split('/+/')
        repo_url = parts[0]
        if len(parts) < 2:
            raise ValueError(f"Invalid GoB URL format: {url} - missing '/+/'")
        path_parts = parts[1].split('/', 1)
        branch = path_parts[0]
        file_path = path_parts[1]
        return repo_url, branch, file_path
    except Exception as e:
        raise ValueError(f"Error parsing URL {url}: {e}")

def replace_non_inclusive_language(file_path):
    """Replaces non-inclusive terms to align with Chromium's guidelines.

    This function is called after fetching external documentation to ensure that
    the cached files pass the Chromium presubmit check for inclusive language.
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Perform case-insensitive replacements.
        content = content.replace("whitelist", "allowlist")  # nocheck
        content = content.replace("blacklist", "denylist")   # nocheck
        content = content.replace("Whitelist", "Allowlist")  # nocheck
        content = content.replace("Blacklist", "Denylist")   # nocheck
        content = content.replace("master", "main")         # nocheck
        content = content.replace("Master", "Main")         # nocheck

        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"    Applied non-inclusive language replacements to {file_path.name}")
    except Exception as e:
        print(f"    Could not process file {file_path}: {e}")

def fetch_doc_with_git(repo_url, branch, file_path, output_file):
    """Fetches a single file from a git repo using a shallow clone into a temporary directory."""
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)
        print(f"    Cloning {repo_url} (branch: {branch}) into temporary directory {tmp_path}")
        try:
            subprocess.run(
                ['git', 'clone', '--depth', '1', '--branch', branch, '--no-checkout', '--filter=blob:none', repo_url, "."],
                check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=tmp_path, timeout=300
            )
            subprocess.run(
                ['git', 'sparse-checkout', 'init', '--cone'],
                 check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=tmp_path, timeout=60
            )
            subprocess.run(
                ['git', 'sparse-checkout', 'set', file_path],
                 check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=tmp_path, timeout=60
            )
            print(f"    Checking out {file_path}...")
            subprocess.run(
                ['git', 'checkout'],
                 check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=tmp_path, timeout=120
            )
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired, FileNotFoundError) as e:
            print(f"    Git operation failed for {repo_url}: {e}")
            return False

        source_file = tmp_path / file_path
        if source_file.exists():
            print(f"    Copying {file_path} to {output_file}")
            try:
                shutil.copyfile(source_file, output_file)
                replace_non_inclusive_language(output_file)
                return True
            except Exception as e:
                print(f"    Error copying file: {e}")
                return False
        else:
            print(f"    Error: File {file_path} not found in repository after sparse checkout.")
            return False
    return False

def fetch_and_cache_docs(manifest, base_output_dir, force=False):
    """Fetches documents from URLs specified in the manifest."""
    print(f"Starting doc refresh. Base output dir: {base_output_dir}")
    base_output_dir.mkdir(parents=True, exist_ok=True)

    successes = []
    failures = []

    remote_docs = manifest.get("remote_documents", [])
    if not remote_docs:
        print("No remote documents to fetch.")
        return

    for doc in remote_docs:
        source_url = doc.get("source_url")
        cached_path = doc.get("cached_path")
        subdir = doc.get("subdir")
        description = doc.get("description", "No description")

        if not source_url or not cached_path or not subdir:
            print(f"  Skipping invalid entry (missing source_url, cached_path, or subdir): {doc}")
            failures.append(f"{description} (Invalid Manifest Entry)")
            continue

        output_dir = base_output_dir / subdir
        output_dir.mkdir(parents=True, exist_ok=True)
        output_file = output_dir / cached_path

        print(f"  Processing: {description} -> {subdir}/{cached_path}")
        print(f"    Source URL: {source_url}")

        try:
            repo_url, branch, file_path = parse_gob_url(source_url)
            if fetch_doc_with_git(repo_url, branch, file_path, output_file):
                successes.append(f"{subdir}/{cached_path}")
            else:
                failures.append(f"{subdir}/{cached_path} (Fetch Failed)")
        except ValueError as e:
            print(f"    Skipping {source_url}: {e}")
            failures.append(f"{subdir}/{cached_path} (URL Parse Error)")
        except Exception as e:
            print(f"    An unexpected error occurred for {source_url}: {e}")
            failures.append(f"{subdir}/{cached_path} (Unexpected Error)")
        print("")

    print("--- Refresh Summary ---")
    print(f"Successfully updated: {len(successes)}")
    print(f"Failed: {len(failures)}")
    if failures:
        print("\nFailed documents:")
        for f in failures:
            print(f"  - {f}")
        if not force:
            sys.exit(1)

def main():
    parser = argparse.ArgumentParser(
        description="Refresh the cached external documentation.")
    parser.add_argument(
        '--force',
        action='store_true',
        help='Continue and exit successfully even if some documents fail to update.'
    )
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    base_output_dir = script_dir
    git_root = script_dir.parent.parent

    print(f"Base output directory: {base_output_dir}")
    print(f"Assumed git root: {git_root}")

    if not (git_root / ".git").exists():
         print(f"Error: Git root not found at {git_root}. Please run this script from within the Chromium source tree.")
         sys.exit(1)

    fetch_and_cache_docs(DOCUMENT_MANIFEST, base_output_dir, args.force)

    print("Document refresh complete.")
    try:
        print(f"Adding changes in {script_dir.relative_to(git_root)} to git index...")
        subprocess.run(['git', 'add', str(script_dir.relative_to(git_root))], check=True, cwd=git_root)
        print("Changes added to git index.")
        print("Please review and commit the changes.")
    except Exception as e:
        print(f"An error occurred while running git add: {e}")

if __name__ == "__main__":
    main()
