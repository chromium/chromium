#!/usr/bin/env python3
import subprocess
import re
import sys

# Regular expression to extract email addresses
email_pattern = re.compile(r'<(.+?)>')


# Function to check if the email belongs to the company
def is_googler_email(email):
    return '@chromium.org' in email or '@google.com' in email


# Function to process the logs and count reviewers and authors
def process_logs(since_date):
    # Start a subprocess to stream the git log output
    proc = subprocess.Popen(['git', 'log', '--since={}'.format(since_date)],
                            stdout=subprocess.PIPE,
                            text=True)

    # Variables to keep track of the current commit being processed
    current_commit_hash = None
    author_email = None
    contributors_count = 0
    googler_contributors_count = 0
    rubber_stamp = False
    owners_override = False

    # Process each line from the git log output
    for line in proc.stdout:
        stripped_line = line.strip()
        if line.startswith('commit '):
            # If we're starting a new commit, output the data for the previous commit
            if current_commit_hash is not None:
                print(
                    f"{current_commit_hash},{author_email},{contributors_count},"
                    f"{googler_contributors_count},{rubber_stamp},{owners_override}"
                )

            # Reset counters for the new commit
            current_commit_hash = stripped_line.split()[1]
            author_email = None
            contributors_count = 0
            googler_contributors_count = 0
            rubber_stamp = False
            owners_override = False
        elif stripped_line.startswith('Author:') or stripped_line.startswith(
                'Reviewed-by:'):
            contributors_count += 1
            match = email_pattern.search(line)
            if match:
                email = match.group(1)
                if stripped_line.startswith('Author:'):
                    author_email = email
                if is_googler_email(email):
                    googler_contributors_count += 1
        elif stripped_line.startswith('Bot-Commit: Rubber Stamper'):
            rubber_stamp = True
        elif stripped_line.startswith('Owners-Override:'):
            rubber_stamp = True

    # Don't forget to output the last commit
    if current_commit_hash is not None:
        print(f"{current_commit_hash},{author_email},{contributors_count},"
              f"{googler_contributors_count},{rubber_stamp},{owners_override}")

    # Close the subprocess
    proc.stdout.close()
    proc.wait()


# Main function
def main():
    if len(sys.argv) != 2:
        print("Usage: reviewer-counts.py <since_date>")
        sys.exit(1)

    since_date = sys.argv[1]
    print("Commit Hash,Author,Contributors,Google Contributors,Rubber Stamp,"
          "Owners Override")
    process_logs(since_date)


if __name__ == "__main__":
    main()
