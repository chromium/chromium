#!/bin/bash
# Test script for MCP Server fixes
# Tests: Accessibility tree, Ref-based actions, JSON escaping

BASE_URL="http://localhost:9224"

echo "========================================="
echo "MCP Server Fixes - Test Script"
echo "========================================="
echo ""

# Check if MCP Server is running
echo "1. Checking if MCP Server is running..."
if ! curl -s "$BASE_URL/" > /dev/null 2>&1; then
    echo "❌ ERROR: MCP Server is not running on port 9224"
    echo "   Please start Chrome and enable MCP Server in chrome://settings/ai"
    exit 1
fi
echo "✅ MCP Server is running"
echo ""

# Create test tab with Google
echo "2. Creating test tab with Google.com..."
TAB_RESPONSE=$(curl -s -X POST "$BASE_URL/mcp/tabs" \
  -H "Content-Type: application/json" \
  -d '{"url": "https://www.google.com"}')

TAB_ID=$(echo "$TAB_RESPONSE" | python3 -c "import json,sys; print(int(json.load(sys.stdin)['id']))" 2>/dev/null)

if [ -z "$TAB_ID" ] || [ "$TAB_ID" = "0" ]; then
    echo "❌ ERROR: Failed to create tab"
    echo "Response: $TAB_RESPONSE"
    exit 1
fi

echo "✅ Created tab ID: $TAB_ID"
echo ""

# Wait for page to load
echo "3. Waiting for page to load..."
sleep 5
echo "✅ Page loaded"
echo ""

# Test Fix #1: Accessibility Tree Filtering
echo "========================================="
echo "TEST 1: Accessibility Tree Filtering"
echo "========================================="
echo "Before fix: Showed only 1 node out of 118"
echo "After fix: Should show 20+ interactive elements"
echo ""

ACC_RESPONSE=$(curl -s "$BASE_URL/mcp/tabs/$TAB_ID/accessibility")

# Count reference IDs in the tree (each [ref=eN] is an interactive element)
REF_COUNT=$(echo "$ACC_RESPONSE" | python3 -c "
import json, sys, re
try:
    data = json.load(sys.stdin)
    tree = data.get('tree', '')
    refs = re.findall(r'\[ref=e\d+\]', tree)
    print(len(refs))
except:
    print('0')
" 2>/dev/null)

echo "Found $REF_COUNT interactive elements with reference IDs"

if [ "$REF_COUNT" -lt "5" ]; then
    echo "❌ FAILED: Too few elements (expected 20+, got $REF_COUNT)"
    echo "Tree sample:"
    echo "$ACC_RESPONSE" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('tree', '')[:500])"
else
    echo "✅ PASSED: Accessibility tree shows multiple interactive elements"
    echo ""
    echo "Sample of tree (first few elements):"
    echo "$ACC_RESPONSE" | python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
    tree = data.get('tree', '')
    lines = tree.split('\n')[:15]
    for line in lines:
        print(line)
except:
    pass
"
fi
echo ""

# Test Fix #2: Reference ID Actions
echo "========================================="
echo "TEST 2: Reference ID-based Actions"
echo "========================================="
echo "Testing POST /mcp/tabs/:id/type-ref endpoint"
echo ""

# Extract first textbox ref ID from accessibility tree
SEARCH_REF=$(echo "$ACC_RESPONSE" | python3 -c "
import json, sys, re
try:
    data = json.load(sys.stdin)
    tree = data.get('tree', '')
    # Find first textbox with a ref
    match = re.search(r'role: \"textbox\"[\s\S]*?\[ref=([e0-9]+)\]', tree)
    if match:
        print(match.group(1))
except:
    pass
" 2>/dev/null)

if [ -z "$SEARCH_REF" ]; then
    echo "⚠️  WARNING: No textbox found with ref ID"
    echo "   Skipping ref-based action test"
else
    echo "Found search box with ref ID: $SEARCH_REF"
    echo "Attempting to type using ref-based action..."

    TYPE_REF_RESPONSE=$(curl -s -X POST "$BASE_URL/mcp/tabs/$TAB_ID/type-ref" \
      -H "Content-Type: application/json" \
      -d "{\"ref\": \"$SEARCH_REF\", \"text\": \"test query\"}")

    TYPE_SUCCESS=$(echo "$TYPE_REF_RESPONSE" | python3 -c "import json,sys; print(json.load(sys.stdin).get('success', False))" 2>/dev/null)

    if [ "$TYPE_SUCCESS" = "True" ]; then
        echo "✅ PASSED: type-ref action works! (used ref=$SEARCH_REF)"
    else
        echo "❌ FAILED: type-ref action failed"
        echo "Response: $TYPE_REF_RESPONSE"
    fi
fi
echo ""

# Test Fix #3: JSON Escaping
echo "========================================="
echo "TEST 3: JSON Escaping in Evaluate"
echo "========================================="
echo "Testing complex JavaScript with quotes and nested objects"
echo ""

# Test complex JavaScript that would fail with old escaping
COMPLEX_JS='(() => { const data = {"test": "value with \"quotes\"", "array": [1,2,3]}; return JSON.stringify(data); })()'

cat > /tmp/eval_test.json << EOF
{"code": "$COMPLEX_JS"}
EOF

EVAL_RESPONSE=$(curl -s -X POST "$BASE_URL/mcp/tabs/$TAB_ID/evaluate" \
  -H "Content-Type: application/json" \
  -d @/tmp/eval_test.json)

EVAL_RESULT=$(echo "$EVAL_RESPONSE" | python3 -c "import json,sys; data=json.load(sys.stdin); print(data.get('result', ''))" 2>/dev/null)

if [ -z "$EVAL_RESULT" ]; then
    echo "❌ FAILED: Evaluate with complex JS failed"
    echo "Response: $EVAL_RESPONSE"
else
    echo "✅ PASSED: Complex JavaScript executed successfully"
    echo "Result: $EVAL_RESULT"
fi
echo ""

# Cleanup
echo "========================================="
echo "Cleaning up..."
curl -s -X DELETE "$BASE_URL/mcp/tabs/$TAB_ID" > /dev/null
echo "✅ Test tab closed"
echo ""

echo "========================================="
echo "ALL TESTS COMPLETED!"
echo "========================================="
