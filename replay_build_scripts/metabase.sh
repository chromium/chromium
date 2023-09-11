buildkite-agent artifact download build_id/linux/x86_64/build_id ./
BUILD_ID=$(cat build_id/linux/x86_64/build_id)

echo 'Running metabase tests on GitHub with inputs: {"ref":"master","inputs":{"chromium-build-id":"'${BUILD_ID}'"}}'

curl -L -s \
    -X POST \
    -H "Accept: application/vnd.github+json" \
    -H "Authorization: Bearer ${GITHUB_AUTH_SECRET}" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/replayio-public/metabase/actions/workflows/e2e-tests.yml/dispatches \
    -d '{"ref":"master","inputs":{"chromium-build-id":"'${BUILD_ID}'"}}'
sleep 5
curl -L -s \
    -X GET \
    -H "Accept: application/vnd.github+json" \
    -H "Authorization: Bearer ${GITHUB_AUTH_SECRET}" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/replayio-public/metabase/actions/runs\?event\=workflow_dispatch | jq -r '"Metabase Test Run: " + (.workflow_runs[0].html_url // "Not Found")' | buildkite-agent annotate --context tests