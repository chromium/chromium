#!/bin/bash

set -eu

if [ -n "${BUILDKITE-}" ]; then
    buildkite-agent artifact download build_id/linux/x86_64/build_id ./
    BUILD_ID=$(cat build_id/linux/x86_64/build_id)

    if [ -z "${BUILD_ID}" ]; then
        echo "Unable to retrieve build id from buildkite agent"
        exit 1
    fi
    RUN_ID="$BUILDKITE_PIPELINE_SLUG/$BUILDKITE_BUILD_NUMBER"
else
    BUILD_ID=$1
    if [ -z "${BUILD_ID}" ]; then
        echo "build ID required as first argument";
        exit 1
    fi
    RUN_ID=test-$(uuidgen)
fi

if [ -z "${GITHUB_AUTH_SECRET}" ]; then
    echo "GITHUB_AUTH_SECRET is not set in environment"
    exit 1
fi


# TODO(toshok) when/if the devtools PR (https://github.com/replayio/devtools/pull/10001) lands, change the ref here to `main`
INPUTS='{"ref":"main","inputs":{"runId":"'${RUN_ID}'","linuxBuildFile":"'${BUILD_ID}'.tar.xz"}}'
echo "Running devtools tests on GitHub with inputs: ${INPUTS}"

curl -L -s \
    -X POST \
    -H "Accept: application/vnd.github+json" \
    -H "Authorization: Bearer ${GITHUB_AUTH_SECRET}" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/replayio/devtools/actions/workflows/test.yml/dispatches \
    -d ${INPUTS}

# and now we wait for a maximum of ITER_COUNT, sleeping SLEEP_SEC_PER_ITER per iteration, polling
# the GH api to get the build status.

# these values should give us ~20 minutes of wait time, which should be wayyy more than enough.
SLEEP_SEC_PER_ITER=60
ITER_COUNT=20

EXPECTED_RUN_NAME="Run test suites - run id ${RUN_ID}"
echo "Expected workflow run name: ${EXPECTED_RUN_NAME}"

iter=0
url_annotation_done=""

while [[ $iter -le $ITER_COUNT ]]; do
  iter=$(( iter + 1 ))
  echo "Iteration ${iter}, sleeping for ${SLEEP_SEC_PER_ITER} seconds..."
  sleep $SLEEP_SEC_PER_ITER

  RUN_JSON=$(curl -L -s \
    -X GET \
    -H "Accept: application/vnd.github+json" \
    -H "Authorization: Bearer ${GITHUB_AUTH_SECRET}" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/replayio/devtools/actions/runs\?event\=workflow_dispatch | jq -r ".workflow_runs[] | select(.name==\"${EXPECTED_RUN_NAME}\")")

  if [[ -n "$RUN_JSON" ]]; then
    RUN_STATUS=$(echo "${RUN_JSON}" | jq -r .status)
    RUN_CONCLUSION=$(echo "${RUN_JSON}" | jq -r .conclusion)
    RUN_URL=$(echo "${RUN_JSON}" | jq -r .html_url)

    if [[ -z "${url_annotation_done}" ]]; then
      url_annotation_done="yes"
      echo "Github workflow run url: ${RUN_URL}"
      if [ -n "${BUILDKITE}" ]; then
        buildkite-agent annotate --context tests "${RUN_URL}"
      fi
    fi

    if [[ $RUN_STATUS == "completed" ]]; then
      echo "Github workflow completed with conclusion: ${RUN_CONCLUSION}"
      if [[ $RUN_CONCLUSION == "success" ]]; then
        # reflect the success status back to build kite
        exit 0
      else
        exit -1
      fi
    else
      echo "Run status was ${RUN_STATUS}, continuing to poll..."
    fi
  else
    echo "Run '${EXPECTED_RUN_NAME}' not present in workflow run list"
  fi
done

echo "GH workflow run didn't complete in time.  failing this step."
echo "If builds need to take longer, increase the iteration count or sleep time per iteration"
exit -1
