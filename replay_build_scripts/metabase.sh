if [ -n "${BUILDKITE}" ]; then
    buildkite-agent artifact download build_id/linux/x86_64/build_id ./
    BUILD_ID=$(cat build_id/linux/x86_64/build_id)

    if [ -z "${BUILD_ID}" ]; then
        echo "Unable to retrieve build id from buildkite agent"
        exit 1
    fi
else
    BUILD_ID=$1
    if [ -z "${BUILD_ID}" ]; then
        echo "build ID required as first argument";
        exit 1
    fi
fi

if [ -z "${GITHUB_AUTH_SECRET}" ]; then
    echo "GITHUB_AUTH_SECRET is not set in environment"
    exit 1
fi

FOLDERS=""
if [ -n "$2" ]; then
    IFS=',' read -ra array <<< "$2"

    # Create a JSON representation of the array
    FOLDERS=",\"folders\":\"["
    for element in "${array[@]}"; do
        FOLDERS+="\\\"$element\\\","
    done
    # Remove the trailing comma and add closing square bracket
    FOLDERS="${FOLDERS%,}]\""
fi;

INPUTS='{"ref":"master","inputs":{"chromium-build-id":"'${BUILD_ID}'"'${FOLDERS}'}}'
echo 'Running metabase tests on GitHub with inputs: '${INPUTS}

curl -L -s \
    -X POST \
    -H "Accept: application/vnd.github+json" \
    -H "Authorization: Bearer ${GITHUB_AUTH_SECRET}" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/replayio-public/metabase/actions/workflows/e2e-tests.yml/dispatches \
    -d ${INPUTS}
sleep 5
GH_URL=$(curl -L -s \
    -X GET \
    -H "Accept: application/vnd.github+json" \
    -H "Authorization: Bearer ${GITHUB_AUTH_SECRET}" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/replayio-public/metabase/actions/runs\?event\=workflow_dispatch | jq -r '"Metabase Test Run: " + (.workflow_runs[0].html_url // "Not Found")')

echo "\n${GH_URL}"
if [ -n "${BUILDKITE}" ]; then
    buildkite-agent annotate --context tests "${GH_URL}"
fi