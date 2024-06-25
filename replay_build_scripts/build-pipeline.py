import json


def read_commit_hash(file_path):
    """Reads the commit hash from the given file."""
    with open(file_path, "r") as file:
        return file.read().strip()


def generate_buildkite_pipeline(backend_commit_hash):
    """Generates a Buildkite pipeline with a dynamic commit hash in JSON format."""

    # driver_revision is the first 12 characters of the commit hash.
    driver_revision = backend_commit_hash.split()[0][:12]
    pipeline = {
        "steps": [
            {
                "trigger": "build-driver-linker",
                "key": "build-driver-linker",
                "build": {
                    "commit": backend_commit_hash,
                    "message": "Triggered from chromium: ${BUILDKITE_MESSAGE}",
                },
            },
            {
                "trigger": "chromium-build",
                "build": {
                    "env": {
                        "DRIVER_REVISION": driver_revision,
                        "REPLAY_BACKEND_REV": backend_commit_hash,
                    },
                    "message": "${BUILDKITE_MESSAGE}",
                    "branch": "${BUILDKITE_BRANCH}",
                    "commit": "${BUILDKITE_COMMIT}",
                },
                "depends_on": ["build-driver-linker"],
            },
        ]
    }
    return json.dumps(pipeline, indent=4)


def main():
    backend_commit_hash = read_commit_hash("REPLAY_BACKEND_REV")
    pipeline_json = generate_buildkite_pipeline(backend_commit_hash)
    print(pipeline_json)


if __name__ == "__main__":
    main()
